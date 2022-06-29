#include "jdpico.h"
#include "hardware/irq.h"

#define ALARM_NUM 0
uint16_t tim_max_sleep;

static cb_t timer_cb;

static void tim_handler(uint num) {
    (void)num;
    cb_t f = timer_cb;
    if (f) {
        target_disable_irq();
        uint32_t tal = timer_hw->alarm[ALARM_NUM];
        uint32_t now = timer_hw->timerawl;
        if (((tal - (now + 2)) >> 29) == 0) {
            // re-arm the alarm
            timer_hw->alarm[ALARM_NUM] = tal;
            DMESG("timer problem: al=%u now=%u", tal, now);
            target_enable_irq();
            return;
        }
        target_enable_irq();

        timer_cb = NULL;
        f();
    }
}

static inline uint harware_alarm_irq_number(uint alarm_num) {
    return TIMER_IRQ_0 + alarm_num;
}

void tim_init() {
    uint irq_num = harware_alarm_irq_number(ALARM_NUM);
    ram_irq_set_priority(irq_num, IRQ_PRIORITY_TIM);
    ram_irq_set_enabled(irq_num, true);

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

REAL_TIME_FUNC
void target_wait_us(uint32_t n) {
    uint32_t start = timer_hw->timerawl;
    while (timer_hw->timerawl - start < n)
        ;
}
