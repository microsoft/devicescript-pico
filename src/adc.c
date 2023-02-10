#include "jdpico.h"
#include "hardware/adc.h"

static int get_adc_ch(int pin) {
    pin -= 26;
    if (pin < 0 || pin > 3)
        return -1;
    return pin;
}

bool adc_can_read_pin(uint8_t pin) {
    return get_adc_ch(pin) != -1;
}

static bool adc_inited;
static void init(void) {
    if (!adc_inited) {
        adc_inited = true;
        adc_init();
    }
}

uint16_t adc_read_pin(uint8_t pin) {
    init();
    adc_select_input(get_adc_ch(pin));
    return adc_read() << 4;
}

uint16_t adc_read_temp(void) {
    init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);
#define VOLTS 3.3
    int adc = adc_read();
    adc_set_temp_sensor_enabled(false);

    int shifted_temp =
        (27 << 12) - (adc - (int)((1 << 12) * 0.706 / VOLTS)) * (int)(VOLTS / 0.001721);
    return shifted_temp >> 12;
}