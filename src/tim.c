#include "jdpico.h"
#include "hardware/irq.h"

#define ALARM_NUM 0
uint16_t tim_max_sleep;

static cb_t timer_cb;
static volatile uint32_t int0;
static volatile uint32_t int1;
static volatile uint32_t int2;
static volatile uint32_t int3;
static volatile uint32_t last_set_now;
static volatile uint32_t last_set_iter;
static volatile uint32_t last_delay;
static volatile uint32_t last_delay_now;

REAL_TIME_FUNC
static void tim_handler(void) {
    timer_hw->intr = 1u << ALARM_NUM;

    cb_t f = timer_cb;
    if (f) {
        target_disable_irq();
        uint32_t tal = timer_hw->alarm[ALARM_NUM];
        uint32_t now = timer_hw->timerawl;
        if (((tal - (now + 5)) >> 29) == 0) {
            // re-arm the alarm
            timer_hw->alarm[ALARM_NUM] = tal;
            last_delay = tal;
            last_delay_now = now;
            int2 = timer_hw->intr;
            DMESG("timer problem: al=%u now=%u", tal, now);
            // jd_panic();
            target_enable_irq();
            return;
        }
        target_enable_irq();

        last_delay = 0;
        timer_cb = NULL;
        f();
    }
}

static inline uint harware_alarm_irq_number(uint alarm_num) {
    return TIMER_IRQ_0 + alarm_num;
}

void tim_init() {
    uint irq_num = harware_alarm_irq_number(ALARM_NUM);

    hardware_alarm_claim(ALARM_NUM);

    irq_set_exclusive_handler(irq_num, tim_handler);
    ram_irq_set_priority(irq_num, IRQ_PRIORITY_TIM);
    ram_irq_set_enabled(irq_num, true);

    hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);
}

REAL_TIME_FUNC
void tim_set_timer(int delta, cb_t cb) {
    if (delta < 10)
        delta = 10;

    if (tim_max_sleep && delta == 10000)
        delta = tim_max_sleep;

    target_disable_irq();
    timer_cb = cb;
    timer_hw->alarm[ALARM_NUM] = (uint32_t)tim_get_micros() + delta;
    // clear any pending IRQ
    timer_hw->intr = 1u << ALARM_NUM; 
    irq_clear(harware_alarm_irq_number(ALARM_NUM));
    target_enable_irq();
}

REAL_TIME_FUNC
uint64_t tim_get_micros() {
    // Need to make sure that the upper 32 bits of the timer
    // don't change, so read that first
    uint32_t hi = timer_hw->timerawh;
    uint32_t lo;
    do {
        // Read the lower 32 bits
        lo = timer_hw->timerawl;
        // Now read the upper 32 bits again and
        // check that it hasn't incremented. If it has loop around
        // and read the lower 32 bits again to get an accurate value
        uint32_t next_hi = timer_hw->timerawh;
        if (hi == next_hi)
            break;
        hi = next_hi;
    } while (true);
    return ((uint64_t)hi << 32u) | lo;
}

REAL_TIME_FUNC
void target_wait_us(uint32_t n) {
    uint32_t start = timer_hw->timerawl;
    while (timer_hw->timerawl - start < n)
        ;
}
