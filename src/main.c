#include "jdpico.h"

int main() {
    platform_init();

    const uint LED_PIN = 11;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    while (true) {
        gpio_put(LED_PIN, 0);
        target_wait_us(50000);
        gpio_put(LED_PIN, 1);
        target_wait_us(950000);
    }
}
