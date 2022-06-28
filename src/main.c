#include "jdpico.h"

uint32_t now;

const char app_dev_class_name[] = "RP2040 Jacscript 59 v0.1";
#define DEV_CLASS 0x35a678a3

uint32_t app_get_device_class(void) {
    return DEV_CLASS;
}

#ifdef PIN_PWR_EN
static power_config_t pwr_cfg = {
    .pin_fault = PIN_PWR_FAULT, // active low
    .pin_en = PIN_PWR_EN,
    .pin_pulse = NO_PIN,
    .en_active_high = 1,
    .fault_ignore_ms = 100, // there 4.7uF cap that takes time to charge
};
#endif

void app_init_services(void) {
#ifdef PIN_PWR_EN
    power_init(&pwr_cfg);
#endif
    jd_role_manager_init();
    init_jacscript_manager();
}

int main() {
    platform_init();
    jd_init();

    while (true) {
        jd_process_everything();
        if (!jd_rx_has_frame())
            target_wait_us(10000);
    }
}
