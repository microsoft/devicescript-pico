#include "jdpico.h"

uint32_t now;

const char app_dev_class_name[] = DEV_CLASS_NAME;

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
    init_devicescript_manager();

#if WIFI_SUPPORTED
    wifi_init();
#endif

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
