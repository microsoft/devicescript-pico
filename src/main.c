#include "jdpico.h"

#include <stdio.h>

uint32_t now;

void app_init_services(void) {
    devs_service_full_init(NULL);

    #if 0
    // these are now started with startHidMouse() etc
    hidkeyboard_init();
    hidmouse_init();
    hidjoystick_init();
    #endif

    if (i2c_init_() == 0) {
        jd_scan_all();
        i2cserv_init();
    }
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

    while (true) {
        jd_process_everything();
        jd_set_max_sleep(1000); // TODO?
#if JD_WIFI
        jd_tcpsock_process();
#endif
        if (dmesg_to_stdout())
            continue;
        if (!jd_rx_has_frame())
            __asm volatile("wfe");
    }
}
