#include "jdpico.h"
#include "hardware/pwm.h"

uint8_t jd_pwm_init(uint8_t pin, uint32_t period, uint32_t duty, uint8_t prescaler) {
    pwm_config c = pwm_get_default_config();
    pwm_config_set_clkdiv_int(&c, prescaler);
    pwm_config_set_wrap(&c, period - 1);

    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_init(slice_num, &c, true);

    gpio_set_function(pin, GPIO_FUNC_PWM);

    return pin;
}

void jd_pwm_set_duty(uint8_t pwm_id, uint32_t duty) {
    pwm_set_gpio_level(pwm_id, duty);
}

void jd_pwm_enable(uint8_t pwm_id, bool enabled) {
    if (enabled)
        gpio_set_function(pwm_id, GPIO_FUNC_PWM);
    else
        gpio_init(pwm_id);
}

void pin_set(int pin, int v) {
    if ((uint8_t)pin != NO_PIN)
        gpio_put(pin, v);
}

void pin_setup_output(int pin) {
    if ((uint8_t)pin != NO_PIN)
        gpio_set_dir(pin, 1);
}

int pin_get(int pin) {
    if ((uint8_t)pin == NO_PIN)
        return -1;
    return gpio_get(pin);
}

void pin_setup_input(int pin, int pull) {
    if ((uint8_t)pin == NO_PIN)
        return;
    pin_set_pull(pin, pull);
    gpio_set_dir(pin, 0);
}

void pin_set_pull(int pin, int pull) {
    if ((uint8_t)pin == NO_PIN)
        return;
    gpio_set_pulls(pin, pull > 0, pull < 0);
}

void pin_setup_analog_input(int pin) {
    if ((uint8_t)pin == NO_PIN)
        return;
    pin_set_pull(pin, PIN_PULL_NONE);
    gpio_deinit(pin);
}

void pwr_enter_no_sleep(void) {}
void pwr_enter_tim(void) {}
void pwr_leave_tim(void) {}
