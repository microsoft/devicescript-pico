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
    static char buf[128];

    if (!uart_log_can_write())
        return true; // wait!

    unsigned len = jd_dmesg_read(buf, sizeof(buf), &dmesg_ptr);
    if (len) {
        uart_log_write(buf, len);
        return true;
    }
    return false;
}

int main() {
    uart_log_init();

    platform_init();
    jd_init();

    tim_max_sleep = 1000;

    while (true) {
        jd_process_everything();
#if JD_WIFI
        jd_tcpsock_process();
#endif
        if (dmesg_to_stdout())
            continue;
        if (!jd_rx_has_frame())
            __asm volatile("wfe");
    }
}
