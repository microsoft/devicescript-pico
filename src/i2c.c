#include "jdpico.h"

#define LOG_TAG "i2c"
#include "devs_logging.h"

#undef i2c_init
#include "hardware/i2c.h"

static i2c_inst_t *i2c_inst;

#define I2C_INST(pin) (((pin) >> 1) & 1 ? i2c1 : i2c0)

int i2c_init_(void) {
    uint8_t sda, scl;

    if (i2c_inst)
        return 0;

    sda = dcfg_get_pin("i2c.sda");
    scl = dcfg_get_pin("i2c.scl");
    if (sda == 0xff || scl == 0xff)
        return -1;

    if (I2C_INST(sda) != I2C_INST(scl)) {
        LOG("inst(SDA)!=inst(SCL)");
        return -2;
    }
    if ((sda & 1) || !(scl & 1)) {
        LOG("sda/scl invalid");
        return -3;
    }

    i2c_inst = I2C_INST(sda);
    int khz = dcfg_get_i32("i2c.khz", 100);

    i2c_init(i2c_inst, khz * 1000);

    gpio_set_function(sda, GPIO_FUNC_I2C);
    gpio_set_function(scl, GPIO_FUNC_I2C);
    gpio_pull_up(sda);
    gpio_pull_up(scl);

    return 0;
}

int i2c_read_ex(uint8_t device_address, void *dst, unsigned len) {
    if (!i2c_inst)
        return -1;
    int r = i2c_read_blocking(i2c_inst, device_address, dst, len, 0);
    return r < 0 ? r : 0;
}

int i2c_write_ex2(uint8_t device_address, const void *src, unsigned len, const void *src2,
                  unsigned len2, bool repeated) {
    if (!i2c_inst)
        return -1;

    int r = i2c_write_blocking(i2c_inst, device_address, src, len, repeated || len2 > 0);
    if (r < 0)
        return r;
    if (len2 > 0)
        r = i2c_write_blocking(i2c_inst, device_address, src2, len2, repeated);
    return r < 0 ? r : 0;
}
