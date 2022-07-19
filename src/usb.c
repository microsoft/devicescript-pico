#include "jdpico.h"
#include "tusb.h"

#include "interfaces/jd_usb.h"

#define LOG(msg, ...) DMESG("usb: " msg, ##__VA_ARGS__)
// #define LOGV(msg, ...) DMESG("usb: " msg, ##__VA_ARGS__)
#define LOGV JD_NOLOG
#define CHK JD_CHK

static volatile uint8_t fq_scheduled;
static uint32_t dmesg_ptr;
static uint32_t dmesg_timer;
static bool is_connected;

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

bool usb_is_connected() {
    return tud_cdc_connected();
}

void usb_process(void) {
    tud_task();
    hid_process();

    if (!tud_cdc_connected()) {
        is_connected = 0;
        return;
    }

    while (tud_cdc_n_write_available(CDC_ITF) >= 64) {
        uint8_t buf[64];
        int sz = jd_usb_pull(buf);
        if (sz > 0) {
            tud_cdc_n_write(CDC_ITF, buf, sz);
            tud_cdc_n_write_flush(CDC_ITF);
        } else {
            break;
        }
    }
    tud_task();

    if (!is_connected) {
        is_connected = 1;
        LOG("connected");
        dmesg_timer = now + (128 << 10);
        if (!dmesg_timer)
            dmesg_timer = 1;
    }

    if (dmesg_timer && in_past(dmesg_timer))
        dmesg_timer = 0;

    if (dmesg_timer)
        return;

    int space = jd_usb_serial_space();
    if (space > 64) {
        uint32_t curr_ptr = codalLogStore.ptr;
        if (curr_ptr < dmesg_ptr) {
            // the dmesg buffer wrapped-around
            curr_ptr = dmesg_ptr;
            while (curr_ptr < sizeof(codalLogStore.buffer) && codalLogStore.buffer[curr_ptr] != 0)
                curr_ptr++;
            if (curr_ptr == dmesg_ptr) {
                // nothing more to write
                dmesg_ptr = 0;
                curr_ptr = codalLogStore.ptr;
            }
        }

        int towrite = curr_ptr - dmesg_ptr;
        if (towrite > 0) {
            if (towrite > space)
                towrite = space;
            jd_usb_write_serial(codalLogStore.buffer + dmesg_ptr, towrite);
            dmesg_ptr += towrite;
        }
    }
}

void tud_mount_cb(void) {
    LOG("USB connected");
}

void tud_umount_cb(void) {
    LOG("USB dis-connected");
}
