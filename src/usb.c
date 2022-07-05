#include "jdpico.h"
#include "tusb.h"

#define LOG(msg, ...) DMESG("usb: " msg, ##__VA_ARGS__)
// #define LOGV(msg, ...) DMESG("usb: " msg, ##__VA_ARGS__)
#define LOGV JD_NOLOG
#define CHK JD_CHK

void usb_init(void) {
    tusb_init();
}

static uint32_t dmesg_ptr;
static uint32_t dmesg_timer;
static bool is_connected;

void tud_cdc_rx_cb(uint8_t itf) {
    uint8_t buf[64];
    int space = tud_cdc_available();
    if (space >= sizeof(buf)) {
        space = tud_cdc_read(buf, sizeof(buf));
        hf2_recv(buf);
    }
}

bool usb_is_connected() {
    return tud_cdc_connected();
}

int usb_write_space_available() {
    return tud_cdc_write_available();
}

int usb_write(const void *buf, unsigned len) {
    return tud_cdc_write(buf, len) == len ? 0 : -1;
}

void usb_process(void) {
    tud_task();
    hid_process();

    if (!tud_cdc_connected()) {
        is_connected = 0;
        return;
    }

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

    int space = tud_cdc_write_available() - CFG_TUD_CDC_TX_BUFSIZE / 2;
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
            hf2_send_serial(codalLogStore.buffer + dmesg_ptr, towrite, 0);
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
