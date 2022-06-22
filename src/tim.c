#include "jdpico.h"

#define ALARM_NUM 0
uint16_t tim_max_sleep;

static cb_t timer_cb;

static void tim_handler(uint num) {
    (void)num;
    cb_t f = timer_cb;
    timer_cb = NULL;
    if (f)
        f();
}

void tim_init() {
    hardware_alarm_claim(ALARM_NUM);
    hardware_alarm_set_callback(ALARM_NUM, tim_handler);
}

void tim_set_timer(int delta, cb_t cb) {
    if (delta < 10)
        delta = 10;

    if (tim_max_sleep && delta == 10000)
        delta = tim_max_sleep;

    target_disable_irq();
    timer_cb = cb;

    // this is very unlikely to do more than one iteration
    absolute_time_t t;
    do {
        update_us_since_boot(&t, time_us_64() + delta);
    } while (hardware_alarm_set_target(ALARM_NUM, t));
    target_enable_irq();
}

uint64_t tim_get_micros() {
    return time_us_64();
}