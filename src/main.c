#include "jdpico.h"

#include <stdio.h>

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

#if JD_WIFI
    wifi_init();
    wsskhealth_init();
    devscloud_init(&wssk_cloud);
    tsagg_init(&wssk_cloud);
#endif

    hidkeyboard_init();
    hidmouse_init();
    hidjoystick_init();
}

static bool dmesg_to_stdout() {
    static uint32_t dmesg_ptr;
    char buf[64];
    unsigned len = jd_dmesg_read(buf, sizeof(buf), &dmesg_ptr);
    if (len) {
        fwrite(buf, len, 1, stdout);
        return true;
    }
    return false;
}

int main() {
    stdio_init_all();

    platform_init();
    jd_init();

    tim_max_sleep = 1000;

    while (true) {
        jd_process_everything();
        jd_tcpsock_process();
        if (dmesg_to_stdout())
            continue;
        if (!jd_rx_has_frame())
            __asm volatile("wfe");
    }
}
