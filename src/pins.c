#include "jdpico.h"
#include "hardware/pwm.h"
#include "hardware/structs/iobank0.h"

static cb_t exti_cb[NUM_BANK0_GPIOS];
static uint8_t exti_ev[NUM_BANK0_GPIOS];

#define COPY static __attribute__((section(".time_critical.gpio")))

static __force_inline void gpio_acknowledge_irq_(uint gpio, uint32_t events) {
    iobank0_hw->intr[gpio / 8] = events << 4 * (gpio % 8);
}

static __force_inline void _gpio_set_irq_enabled(uint gpio, uint32_t events, bool enabled,
                                                 io_irq_ctrl_hw_t *irq_ctrl_base) {
    // Clear stale events which might cause immediate spurious handler entry
    gpio_acknowledge_irq_(gpio, events);

    io_rw_32 *en_reg = &irq_ctrl_base->inte[gpio / 8];
    events <<= 4 * (gpio % 8);

    if (enabled)
        hw_set_bits(en_reg, events);
    else
        hw_clear_bits(en_reg, events);
}

COPY void gpio_set_irq_enabled_(uint gpio, uint32_t events, bool enabled) {
    // Separate mask/force/status per-core, so check which core called, and
    // set the relevant IRQ controls.
    io_irq_ctrl_hw_t *irq_ctrl_base = &iobank0_hw->proc0_irq_ctrl;
    _gpio_set_irq_enabled(gpio, events, enabled, irq_ctrl_base);
}

COPY void gpio_set_function_(uint gpio, enum gpio_function fn) {
    invalid_params_if(GPIO, gpio >= NUM_BANK0_GPIOS);
    invalid_params_if(GPIO, ((uint32_t)fn << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB) &
                                ~IO_BANK0_GPIO0_CTRL_FUNCSEL_BITS);
    // Set input enable on, output disable off
    hw_write_masked(&padsbank0_hw->io[gpio], PADS_BANK0_GPIO0_IE_BITS,
                    PADS_BANK0_GPIO0_IE_BITS | PADS_BANK0_GPIO0_OD_BITS);
    // Zero all fields apart from fsel; we want this IO to do what the peripheral tells it.
    // This doesn't affect e.g. pullup/pulldown, as these are in pad controls.
    iobank0_hw->io[gpio].ctrl = fn << IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB;
}

COPY void gpio_init_(uint gpio) {
    sio_hw->gpio_oe_clr = 1ul << gpio;
    sio_hw->gpio_clr = 1ul << gpio;
    gpio_set_function_(gpio, GPIO_FUNC_SIO);
}

static __force_inline void gpio_set_pulls_(uint gpio, bool up, bool down) {
    invalid_params_if(GPIO, gpio >= NUM_BANK0_GPIOS);
    hw_write_masked(&padsbank0_hw->io[gpio],
                    (bool_to_bit(up) << PADS_BANK0_GPIO0_PUE_LSB) |
                        (bool_to_bit(down) << PADS_BANK0_GPIO0_PDE_LSB),
                    PADS_BANK0_GPIO0_PUE_BITS | PADS_BANK0_GPIO0_PDE_BITS);
}

REAL_TIME_FUNC
void isr_io_bank0() {
    io_irq_ctrl_hw_t *irq_ctrl_base = &iobank0_hw->proc0_irq_ctrl; // assume io irq only on core0
    for (uint gpio = 0; gpio < NUM_BANK0_GPIOS; gpio++) {
        const io_rw_32 *status_reg = &irq_ctrl_base->ints[gpio / 8];
        uint events = (*status_reg >> 4 * (gpio % 8)) & 0xf;
        if (events) {
            gpio_acknowledge_irq(gpio, events);
            if (exti_cb[gpio])
                exti_cb[gpio]();
        }
    }
}

uint8_t jd_pwm_init(uint8_t pin, uint32_t period, uint32_t duty, uint8_t prescaler) {
    pwm_config c = pwm_get_default_config();
    pwm_config_set_clkdiv_int(&c, prescaler);
    pwm_config_set_wrap(&c, period - 1);

    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_init(slice_num, &c, true);

    gpio_set_function_(pin, GPIO_FUNC_PWM);

    return pin;
}

REAL_TIME_FUNC
void jd_pwm_set_duty(uint8_t pwm_id, uint32_t duty) {
    pwm_set_gpio_level(pwm_id, duty);
}

REAL_TIME_FUNC
void jd_pwm_enable(uint8_t pwm_id, bool enabled) {
    if (enabled)
        gpio_set_function_(pwm_id, GPIO_FUNC_PWM);
    else
        gpio_init_(pwm_id);
}

REAL_TIME_FUNC
void pin_set(int pin, int v) {
    if ((uint8_t)pin != NO_PIN)
        gpio_put(pin, v);
}

REAL_TIME_FUNC
void pin_setup_output(int pin) {
    if ((uint8_t)pin != NO_PIN)
        gpio_set_dir(pin, 1);
}

REAL_TIME_FUNC
int pin_get(int pin) {
    if ((uint8_t)pin == NO_PIN)
        return -1;
    return gpio_get(pin);
}

REAL_TIME_FUNC
void pin_setup_input(int pin, int pull) {
    if ((uint8_t)pin == NO_PIN)
        return;
    pin_set_pull(pin, pull);
    gpio_set_dir(pin, 0);
}

REAL_TIME_FUNC
void pin_set_pull(int pin, int pull) {
    if ((uint8_t)pin == NO_PIN)
        return;
    gpio_set_pulls_(pin, pull > 0, pull < 0);
}

REAL_TIME_FUNC
void pin_setup_analog_input(int pin) {
    if ((uint8_t)pin == NO_PIN)
        return;
    pin_set_pull(pin, PIN_PULL_NONE);
    gpio_set_function_(pin, GPIO_FUNC_NULL);
}

REAL_TIME_FUNC
void pin_setup_output_af(int pin, int af) {
    if ((uint8_t)pin == NO_PIN)
        return;
    gpio_set_function_(pin, af);
}

void pwr_enter_no_sleep(void) {}
void pwr_enter_tim(void) {}
void pwr_leave_tim(void) {}

void exti_set_callback(uint8_t pin, cb_t callback, uint32_t flags) {
    exti_cb[pin] = callback;
    uint8_t events = 0;
    if (flags & EXTI_FALLING)
        events |= GPIO_IRQ_EDGE_FALL;
    if (flags & EXTI_RISING)
        events |= GPIO_IRQ_EDGE_RISE;
    exti_ev[pin] = events;
    gpio_set_irq_enabled_(pin, events, true);
}

void exti_disable(uint8_t pin) {
    gpio_set_irq_enabled(pin, exti_ev[pin], false);
}

void exti_enable(uint8_t pin) {
    gpio_set_irq_enabled(pin, exti_ev[pin], true);
}
