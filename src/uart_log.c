#include "jdpico.h"

#include "hardware/dma.h"
#include "hardware/claim.h"
#include "hardware/uart.h"

#include "pico/stdio.h"
#include "pico/stdio/driver.h"

static int dmachTx = -1;
static bool in_tx;
static uint8_t prev_tx_pin;

static void stdio_uart_out_chars(const char *buf, int length) {
    jd_dmesg_write(buf, length);
}

static int stdio_uart_in_chars(char *buf, int length) {
    return PICO_ERROR_NO_DATA;
}

stdio_driver_t stdio_dmesg = {
    .out_chars = stdio_uart_out_chars,
    .in_chars = stdio_uart_in_chars,
};

void uart_log_init(void) {
    uint8_t tx_pin = dcfg_get_pin("log.pinTX");

    if (tx_pin != NO_PIN && ((tx_pin & 3) != 0)) {
        DMESG("! invalid log.pinTX");
        tx_pin = NO_PIN;
    }

    if (tx_pin == NO_PIN) {
        dmachTx = -2;
    } else {
        dmachTx = dma_claim_unused_channel(true);
        dma_channel_config c = dma_channel_get_default_config(dmachTx);
        channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
        uart_inst_t *inst = (tx_pin >> 2) & 1 ? uart1 : uart0;
        channel_config_set_dreq(&c, uart_get_dreq(inst, true));
        dma_channel_configure(dmachTx, &c, &uart_get_hw(inst)->dr, NULL, 0, false);
        uart_init(inst, dcfg_get_u32("log.baud", 115200));
        gpio_set_function(tx_pin, GPIO_FUNC_UART);
    }

    prev_tx_pin = tx_pin;

    stdio_set_driver_enabled(&stdio_dmesg, true);
}

void uart_log_sync_pin(void) {
    uint8_t tx_pin = dcfg_get_pin("log.pinTX");
    if (tx_pin == prev_tx_pin)
        return;
    if (tx_pin != NO_PIN && ((tx_pin & 3) != 0)) {
        DMESG("! invalid log.pinTX");
        return;
    }
    pin_setup_analog_input(prev_tx_pin);
    prev_tx_pin = tx_pin;
    gpio_set_function(tx_pin, GPIO_FUNC_UART);
}

bool uart_log_can_write(void) {
    return !in_tx;
}

static void tx_handler(void *p) {
    in_tx = 0;
}

void uart_log_write(const void *buf, unsigned size) {
    if (dmachTx == -2)
        return; // disabled
    if (dmachTx == -1)
        uart_log_init();
    JD_ASSERT(!in_tx);
    in_tx = 1;
    dma_set_ch_cb(dmachTx, tx_handler, NULL);
    dma_channel_transfer_from_buffer_now(dmachTx, buf, size);
}
