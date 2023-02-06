#include "jdpico.h"
#include "tusb.h"

#include "interfaces/jd_usb.h"

#define LOG(msg, ...) DMESG("usb: " msg, ##__VA_ARGS__)
// #define LOGV(msg, ...) DMESG("usb: " msg, ##__VA_ARGS__)
#define LOGV JD_NOLOG
#define CHK JD_CHK

static volatile uint8_t fq_scheduled;

#define CDC_ITF 0

void jd_usb_pull_ready(void) {
    // we eagerly fill buffers in usb_process()
}

void tud_cdc_tx_complete_cb(uint8_t itf) {
    jd_usb_pull_ready();
}

void usb_init(void) {
    tusb_init();
}

void tud_cdc_rx_cb(uint8_t itf) {
    uint8_t buf[64];
    int space = tud_cdc_available();
    while (space > 0) {
        space = tud_cdc_read(buf, sizeof(buf));
        jd_usb_push(buf, space);
        space = tud_cdc_available();
    }
}

bool jd_usb_looks_connected() {
    return tud_cdc_connected();
}

void jd_usb_process(void) {
    uint8_t buf[64];

    tud_task();
    hid_process();

    if (!tud_cdc_connected())
        return;

    while (tud_cdc_n_write_available(CDC_ITF) >= 64) {
        int sz = jd_usb_pull(buf);
        if (sz > 0) {
            tud_cdc_n_write(CDC_ITF, buf, sz);
            tud_cdc_n_write_flush(CDC_ITF);
        } else {
            break;
        }
    }
    tud_task();
}

void tud_mount_cb(void) {
    LOG("USB connected");
}

void tud_umount_cb(void) {
    LOG("USB dis-connected");
}
