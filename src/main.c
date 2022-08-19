#include "jdpico.h"

uint32_t now;

#if BRAIN_ID == 59
const char app_dev_class_name[] = "RP2040 Jacscript 59 v0.1";
#define DEV_CLASS 0x35a678a3
#endif

#if BRAIN_ID == 124
const char app_dev_class_name[] = "RP2040 Jacscript 124 v0.1";
#define DEV_CLASS 0x3875e80d
#endif

uint32_t app_get_device_class(void) {
    return DEV_CLASS;
}

#ifdef PWR_CFG
static power_config_t pwr_cfg = {PWR_CFG};
#endif

void app_init_services(void) {
#ifdef PWR_CFG
#ifdef PIN_PWR_N_ILIM_HI
    pin_set(PIN_PWR_N_ILIM_HI, 0);
    pin_setup_output(PIN_PWR_N_ILIM_HI);
#endif
    power_init(&pwr_cfg);
#endif
    jd_role_manager_init();
    init_jacscript_manager();

    hidkeyboard_init();
    hidmouse_init();
    hidjoystick_init();
}

int main() {
    platform_init();
    jd_init();

    while (true) {
        jd_process_everything();
        usb_process();
        if (!jd_rx_has_frame())
            __asm volatile("wfe");
    }
}
