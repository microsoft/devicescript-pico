#include "jdpico.h"
#include "hardware/structs/rosc.h"
#include "hardware/clocks.h"
#include "pico/unique_id.h"
#include "pico/bootrom.h"
#include "services/interfaces/jd_adc.h"

#include <stdlib.h>

static uint64_t _jd_device_id;
static int8_t irq_disabled;

uint32_t hw_random(void) {
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

uint8_t cpu_mhz;

bool is_pico_w;
static uint8_t use_pico_led;
static void pico_w_init(void) {
    pin_setup_analog_input(29);
    if (adc_read_pin(29) < 3264) {
        is_pico_w = 1;
    } else {
        pin_set(25, 0);
        pin_setup_output(25);
        if (adc_read_pin(29) < 3264)
            is_pico_w = 1;
        else
            is_pico_w = 0;
        pin_setup_analog_input(25);
    }
    DMESG("board is: Pico%s", is_pico_w ? " W" : "");
}

bool jd_wifi_available(void) {
    return is_pico_w;
}

void jd_rgbext_init(int type, uint8_t pin) {
    if (type == 100) {
        use_pico_led = pin;
        if (!is_pico_w)
            pin_setup_output(use_pico_led);
    }
}

void jd_rgbext_set(uint8_t r, uint8_t g, uint8_t b) {
    if (!use_pico_led)
        return;
    if (is_pico_w) {
#if JD_WIFI
        pico_w_set_led(r, g, b);
#endif
    } else {
        pin_set(use_pico_led, r > 0 || g > 0 || b > 0);
    }
}

void platform_init(void) {
    DMESG("start!");

    // detect pico-W early - it messes with pins
    if (dcfg_get_i32("led.type", 0) == 100)
        pico_w_init();

    uint32_t r = hw_random();
    DMESG("rand %x", r);
    jd_seed_random(r);

    pico_unique_board_id_t id;
    pico_get_unique_board_id(&id);
    JD_ASSERT(sizeof(_jd_device_id) == sizeof(id));
    memcpy(&_jd_device_id, &id, sizeof(_jd_device_id));

    cpu_mhz = clock_get_hz(clk_sys) / 1000000;

    tim_init();
    uart_init_();

    usb_init();
}

uint64_t hw_device_id(void) {
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
#elif defined(PIN_LED)
    pin_setup_output(PIN_LED);
    pin_setup_output(PIN_LED_GND);
    pin_set(PIN_LED_GND, 0);
    pin_set(PIN_LED, 1);
    target_wait_us(70000);
    pin_set(PIN_LED, 0);
    target_wait_us(70000);
#else
    target_wait_us(140000);
#endif
}

void hw_panic(void) {
    DMESG("HW PANIC!");
    target_disable_irq();
#ifdef JD_INFINITE_PANIC
    for (;;)
        led_panic_blink();
#else
    for (int i = 0; i < 60; ++i) {
        led_panic_blink();
    }
    target_reset();
#endif
}

void reboot_to_uf2(void) {
    reset_usb_boot(0, 0);
}

JD_FAST
void target_enable_irq(void) {
    irq_disabled--;
    if (irq_disabled <= 0) {
        irq_disabled = 0;
        asm volatile("cpsie i" : : : "memory");
    }
}

JD_FAST
void target_disable_irq(void) {
    asm volatile("cpsid i" : : : "memory");
    irq_disabled++;
}

JD_FAST
int target_in_irq(void) {
    return (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0;
}

void __libc_init_array(void) {
    // do nothing - not using static constructors
}

void jd_crypto_get_random(uint8_t *buf, unsigned size) {
    for (unsigned i = 0; i < size; i += 4) {
        uint32_t v = hw_random();
        if (size - i < 4)
            memcpy(buf + i, &v, size - i);
        else
            memcpy(buf + i, &v, 4);
    }
}
