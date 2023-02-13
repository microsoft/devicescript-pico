#include "jdpico.h"

#include "hardware/irq.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "sws.pio.h"

// based on
// https://github.com/lancaster-university/codal-rp2040/blob/master/source/ZSingleWireSerial.cpp

#define COPY __attribute__((section(".time_critical.sws"))) static

#define STATUS_IDLE 0
#define STATUS_TX 0x10
#define STATUS_RX 0x20

#define PIO_BREAK_IRQ 0x2
// #define DEBUG_PIN 6

static int dmachTx = -1;
static int dmachRx = -1;
static uint txprog, rxprog;
static uint8_t smtx = 0;
static uint8_t smrx = 1;
static uint16_t uart_status;
static uint8_t jacdac_pin;

static inline void pio_gpio_init_(PIO pio, uint pin) {
    check_pio_param(pio);
    valid_params_if(PIO, pin < 32);
    pin_setup_output_af(pin, pio == pio0 ? GPIO_FUNC_PIO0 : GPIO_FUNC_PIO1);
}

COPY void pio_sm_set_consecutive_pindirs_(PIO pio, uint sm, uint pin, uint count, bool is_out) {
    check_pio_param(pio);
    check_sm_param(sm);
    valid_params_if(PIO, pin < 32u);
    uint32_t pinctrl_saved = pio->sm[sm].pinctrl;
    uint pindir_val = is_out ? 0x1f : 0;
    while (count > 5) {
        pio->sm[sm].pinctrl =
            (5u << PIO_SM0_PINCTRL_SET_COUNT_LSB) | (pin << PIO_SM0_PINCTRL_SET_BASE_LSB);
        pio_sm_exec(pio, sm, pio_encode_set(pio_pindirs, pindir_val));
        count -= 5;
        pin = (pin + 5) & 0x1f;
    }
    pio->sm[sm].pinctrl =
        (count << PIO_SM0_PINCTRL_SET_COUNT_LSB) | (pin << PIO_SM0_PINCTRL_SET_BASE_LSB);
    pio_sm_exec(pio, sm, pio_encode_set(pio_pindirs, pindir_val));
    pio->sm[sm].pinctrl = pinctrl_saved;
}

COPY void pio_sm_set_pins_with_mask_(PIO pio, uint sm, uint32_t pinvals, uint32_t pin_mask) {
    check_pio_param(pio);
    check_sm_param(sm);
    uint32_t pinctrl_saved = pio->sm[sm].pinctrl;
    while (pin_mask) {
        uint base = (uint)__builtin_ctz(pin_mask);
        pio->sm[sm].pinctrl =
            (1u << PIO_SM0_PINCTRL_SET_COUNT_LSB) | (base << PIO_SM0_PINCTRL_SET_BASE_LSB);
        pio_sm_exec(pio, sm, pio_encode_set(pio_pins, (pinvals >> base) & 0x1u));
        pin_mask &= pin_mask - 1;
    }
    pio->sm[sm].pinctrl = pinctrl_saved;
}

COPY void pio_sm_set_pindirs_with_mask_(PIO pio, uint sm, uint32_t pindirs, uint32_t pin_mask) {
    check_pio_param(pio);
    check_sm_param(sm);
    uint32_t pinctrl_saved = pio->sm[sm].pinctrl;
    while (pin_mask) {
        uint base = (uint)__builtin_ctz(pin_mask);
        pio->sm[sm].pinctrl =
            (1u << PIO_SM0_PINCTRL_SET_COUNT_LSB) | (base << PIO_SM0_PINCTRL_SET_BASE_LSB);
        pio_sm_exec(pio, sm, pio_encode_set(pio_pindirs, (pindirs >> base) & 0x1u));
        pin_mask &= pin_mask - 1;
    }
    pio->sm[sm].pinctrl = pinctrl_saved;
}

COPY void pio_sm_init_(PIO pio, uint sm, uint initial_pc, const pio_sm_config *config) {
    valid_params_if(PIO, initial_pc < PIO_INSTRUCTION_COUNT);
    // Halt the machine, set some sensible defaults
    pio_sm_set_enabled(pio, sm, false);

    if (config) {
        pio_sm_set_config(pio, sm, config);
    } else {
        pio_sm_config c = pio_get_default_sm_config();
        pio_sm_set_config(pio, sm, &c);
    }

    pio_sm_clear_fifos(pio, sm);

    // Clear FIFO debug flags
    const uint32_t fdebug_sm_mask = (1u << PIO_FDEBUG_TXOVER_LSB) | (1u << PIO_FDEBUG_RXUNDER_LSB) |
                                    (1u << PIO_FDEBUG_TXSTALL_LSB) | (1u << PIO_FDEBUG_RXSTALL_LSB);
    pio->fdebug = fdebug_sm_mask << sm;

    // Finally, clear some internal SM state
    pio_sm_restart(pio, sm);
    pio_sm_clkdiv_restart(pio, sm);
    pio_sm_exec(pio, sm, pio_encode_jmp(initial_pc));
}

static inline void rx_done(void) {
    if (uart_status & STATUS_RX) {
        uart_status &= ~STATUS_RX;
        uart_disable();
        jd_rx_completed(0);
    }
}

JD_FAST
static void rx_handler(void *p) {
    rx_done();
}

JD_FAST
static void tx_handler(void *p) {
    // first clear any pending stalls
    pio0->fdebug = (1u << (PIO_FDEBUG_TXSTALL_LSB + smtx));
    // wait for the data to be actually sent - wait for stall
    while (!(pio0->fdebug & (1u << (PIO_FDEBUG_TXSTALL_LSB + smtx))))
        ;

    // send final BRK
    uint8_t pin = jacdac_pin;
    pin_set(pin, 1);
    pin_setup_output(pin);
    pin_setup_output_af(pin, GPIO_FUNC_SIO);
    target_wait_us(1);
    pin_set(pin, 0);
    target_wait_us(12);
    pin_set(pin, 1);
    uart_disable();

    jd_tx_completed(0);
}

JD_FAST
void isr_pio0_0() {
    uint32_t n = pio0->irq;
    pio0->irq = n;
    if (n & PIO_BREAK_IRQ) {
        rx_done();
    }
}

JD_FAST
static void jd_tx_arm_pin(PIO pio, uint sm, uint pin) {
    pio_sm_set_pins_with_mask_(pio, sm, 1u << pin, 1u << pin);
    pio_sm_set_pindirs_with_mask_(pio, sm, 1u << pin, 1u << pin);
    pio_gpio_init_(pio, pin);
}

JD_FAST
static void jd_tx_program_init(PIO pio, uint sm, uint offset, uint pin, uint baud) {
    jd_tx_arm_pin(pio, sm, pin);
    pio_sm_config c = jd_tx_program_get_default_config(offset);
    sm_config_set_out_shift(&c, true, false, 32);
    sm_config_set_out_pins(&c, pin, 1);
    sm_config_set_sideset_pins(&c, pin);
    // sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    // limit the size of the TX fifo - it's filled by DMA anyway and we have to busy-wait for it
    // flush
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_NONE);
    float div = (float)clock_get_hz(clk_sys) / (8 * baud);
    sm_config_set_clkdiv(&c, div);
    pio_sm_init_(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, false); // enable when need
}

JD_FAST
static void jd_rx_arm_pin(PIO pio, uint sm, uint pin) {
#ifdef DEBUG_PIN
    pio_sm_set_pins_with_mask_(pio, sm, 1u << DEBUG_PIN, 1u << DEBUG_PIN); // init high
    pio_sm_set_pindirs_with_mask_(pio, sm, (0u << pin) | (1u << DEBUG_PIN),
                                  (1u << pin) | (1u << DEBUG_PIN));
    pio_gpio_init_(pio, DEBUG_PIN);
    gpio_set_outover(DEBUG_PIN, GPIO_OVERRIDE_NORMAL);
#else
    pio_sm_set_consecutive_pindirs_(pio, sm, pin, 1, false);
#endif
    exti_disable(pin);
    pio_gpio_init_(pio, pin);
    pin_set_pull(pin, PIN_PULL_UP);
}

JD_FAST
static void jd_rx_program_init(PIO pio, uint sm, uint offset, uint pin, uint baud) {
    jd_rx_arm_pin(pio, sm, pin);
    pio_sm_config c = jd_rx_program_get_default_config(offset);
    sm_config_set_in_pins(&c, pin);
    sm_config_set_jmp_pin(&c, pin);
    sm_config_set_in_shift(&c, true, true, 8);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
#ifdef DEBUG_PIN
    sm_config_set_sideset_pins(&c, DEBUG_PIN);
#endif
    float div = (float)clock_get_hz(clk_sys) / (8 * baud);
    sm_config_set_clkdiv(&c, div);
    pio_sm_init_(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, false); // enable when need
}

void uart_init_() {
    uint8_t pin = dcfg_get_pin("jacdac.pin");
    jacdac_pin = pin;
    if (pin == NO_PIN)
        return;

    pin_set(pin, 1);
    pin_setup_output(pin);             // yank it up
    pin_setup_input(pin, PIN_PULL_UP); // and keep it up

    txprog = pio_add_program(pio0, &jd_tx_program);
    rxprog = pio_add_program(pio0, &jd_rx_program);

    uint baudrate = 1000000;
    jd_tx_program_init(pio0, smtx, txprog, pin, baudrate);
    jd_rx_program_init(pio0, smrx, rxprog, pin, baudrate);

    // fixed dma channels
    dmachRx = dma_claim_unused_channel(true);
    dmachTx = dma_claim_unused_channel(true);

    // init dma

    dma_channel_config c = dma_channel_get_default_config(dmachRx);
    channel_config_set_bswap(&c, 1);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_dreq(&c, DREQ_PIO0_RX0 + smrx);
    uint8_t *rxPtr = (uint8_t *)&pio0->rxf[smrx] + 3;
    dma_channel_configure(dmachRx, &c,
                          NULL,  // dest
                          rxPtr, // src
                          0, false);

    c = dma_channel_get_default_config(dmachTx);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_dreq(&c, pio_get_dreq(pio0, smtx, true));
    dma_channel_configure(dmachTx, &c, &pio0->txf[smtx], NULL, 0, false);
    // pio irq for dma rx break handling

    exti_set_callback(pin, jd_line_falling, EXTI_FALLING);

    ram_irq_set_priority(DMA_IRQ_0, IRQ_PRIORITY_UART);
    ram_irq_set_priority(PIO0_IRQ_0, IRQ_PRIORITY_UART);
    ram_irq_set_enabled(PIO0_IRQ_0, true);

    uart_disable();
}

int uart_wait_high(void) {
    int timeout = 50000;
    uint8_t pin = jacdac_pin;
    while (timeout-- > 0 && !pin_get(pin))
        ;
    if (timeout <= 0)
        return -1;
    else
        return 0;
}

void uart_flush_rx() {}

JD_FAST
void uart_disable() {
    uint8_t pin = jacdac_pin;
    if (pin == NO_PIN)
        return;
    uart_status = STATUS_IDLE;
    target_disable_irq();
    dma_hw->abort = (1 << dmachRx) | (1 << dmachTx);
    pin_setup_input(pin, PIN_PULL_UP);
    pin_setup_output_af(pin, GPIO_FUNC_SIO); // release gpio
    pio_sm_set_enabled(pio0, smtx, false);
    pio_sm_set_enabled(pio0, smrx, false);
    pio_set_irq0_source_enabled(pio0, pis_interrupt1, false);
    exti_enable(pin);
    target_enable_irq();
}

// This is extracted to a function to make sure the compiler doesn't
// insert stuff between checking the input pin and setting the mode.
JD_FAST
__attribute__((noinline)) uint32_t gpio_probe_and_set(sio_hw_t *hw, uint32_t pin) {
    return (hw->gpio_oe_set = hw->gpio_in & pin);
}

JD_FAST
int uart_start_tx(const void *data, uint32_t len) {
    uint8_t pin = jacdac_pin;
    if (pin == NO_PIN) {
        jd_tx_completed(0);
        return 0; // pretend no error so the send buffer won't overflow all the time
    }
    exti_disable(pin);
    // We assume EXTI runs at higher priority than us
    // If we got hit by EXTI, before we managed to disable it,
    // the reception routine would have enabled UART in RX mode
    if (uart_status & STATUS_RX) {
        // we don't re-enable EXTI - the RX complete will do it
        return -1;
    }
    JD_ASSERT(uart_status == 0);
    uart_status = STATUS_TX;
    pin_set(pin, 0);
    if (!gpio_probe_and_set(sio_hw, (1 << pin))) {
        // this is equivalent to irq priority when running from EXTI
        target_disable_irq();
        jd_line_falling();
        target_enable_irq();
        return -1;
    }
    target_wait_us(12);
    pin_set(pin, 1);

    target_wait_us(49);

    jd_tx_arm_pin(pio0, smtx, pin);
    pio_sm_set_enabled(pio0, smtx, true);
    dma_set_ch_cb(dmachTx, tx_handler, NULL);
    dma_channel_transfer_from_buffer_now(dmachTx, data, len);

    // clear stall if any
    pio0->fdebug = (1u << (PIO_FDEBUG_TXSTALL_LSB + smtx));

    return 0;
}

JD_FAST
void uart_start_rx(void *data, uint32_t len) {
    JD_ASSERT(jacdac_pin != NO_PIN);
    uart_status = STATUS_RX;
    jd_rx_arm_pin(pio0, smrx, jacdac_pin);
    // jd_rx_program_init(pio0, smrx, rxprog, jacdac_pin, baudrate);
    pio_sm_set_enabled(pio0, smrx, true);

    pio0->irq = PIO_BREAK_IRQ; // clear irq
    dma_set_ch_cb(dmachRx, rx_handler, NULL);
    dma_channel_transfer_to_buffer_now(dmachRx, data, len);
    pio_set_irq0_source_enabled(pio0, pis_interrupt1, true);
}
