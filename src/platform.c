#include "jdpico.h"
#include "hardware/structs/rosc.h"

static uint32_t hw_random(void) {
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

void platform_init(void) {
    jd_seed_random(hw_random());
}