#include "jdpico.h"

uint32_t now;

int main() {
    platform_init();
    jd_status_init();

    jd_glow(JD_GLOW_BRAIN_DISCONNECTED);

    while (true) {
        target_wait_us(10000);
        jd_refresh_now();
        jd_status_process();
    }
}
