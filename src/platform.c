#include "jdpico.h"
#include "hardware/structs/rosc.h"
#include "pico/unique_id.h"
#include "pico/bootrom.h"

#include <stdlib.h>

static uint64_t _jd_device_id;
static int8_t irq_disabled;

static uint32_t hw_random(void) {
    uint8_t buf[16];
    for (int i = 0; i < sizeof(buf); ++i) {
        buf[i] = 0;
        for (int j = 0; j < 8; ++j) {
            buf[i] <<= 1;
            if (rosc_hw->randombit)
                buf[i] |= 1;
        }
    }
    return jd_hash_fnv1a(buf, sizeof(buf));
}

void platform_init(void) {
    DMESG("JD init, %s", app_fw_version);
    uint32_t r = hw_random();
    DMESG("rand %x", r);
    jd_seed_random(r);

    pico_unique_board_id_t id;
    pico_get_unique_board_id(&id);
    JD_ASSERT(sizeof(_jd_device_id) == sizeof(id));
    memcpy(&_jd_device_id, &id, sizeof(_jd_device_id));

    tim_init();
    uart_init_();

    usb_init();
}

int jd_pin_num(void) {
    return PIN_JACDAC;
}

uint64_t jd_device_id(void) {
    return _jd_device_id;
}

void jd_alloc_stack_check(void) {}
void jd_alloc_init(void) {}

void *jd_alloc(uint32_t size) {
    return calloc(size, 1);
}

void jd_free(void *ptr) {
    free(ptr);
}

void *jd_alloc_emergency_area(uint32_t size) {
    return calloc(size, 1);
}

void target_reset(void) {
    NVIC_SystemReset();
}

static void led_panic_blink(void) {
#ifdef PIN_LED_R
    // TODO should we actually PWM?
    pin_setup_output(PIN_LED_R);
    // it doesn't actually matter if LED_RGB_COMMON_CATHODE is defined, as we're just blinking
    pin_set(PIN_LED_R, 0);
    target_wait_us(70000);
    pin_set(PIN_LED_R, 1);
    target_wait_us(70000);
#else
    pin_setup_output(PIN_LED);
    pin_setup_output(PIN_LED_GND);
    pin_set(PIN_LED_GND, 0);
    pin_set(PIN_LED, 1);
    target_wait_us(70000);
    pin_set(PIN_LED, 0);
    target_wait_us(70000);
#endif
}

void hw_panic(void) {
    DMESG("HW PANIC!");
    target_disable_irq();
#ifdef JD_INFINITE_PANIC
    for (;;)
        led_panic_blink();
#else
    for (int i = 0; i < 60000000; ++i) {
        led_panic_blink();
    }
    target_reset();
#endif
}

void reboot_to_uf2(void) {
    reset_usb_boot(0, 0);
}

void target_enable_irq(void) {
    irq_disabled--;
    if (irq_disabled <= 0) {
        irq_disabled = 0;
        asm volatile("cpsie i" : : : "memory");
    }
}

void target_disable_irq(void) {
    asm volatile("cpsid i" : : : "memory");
    irq_disabled++;
}

int target_in_irq(void) {
    return (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0;
}

void __libc_init_array(void) {
    // do nothing - not using static constructors
}