#include "jdpico.h"
#include "uf2hid.h"

// 260 bytes needed for biggest JD packets (with overheads)
#define HF2_BUF_SIZE 260

typedef struct {
    uint16_t size;
    uint8_t serial;
    union {
        uint8_t buf[HF2_BUF_SIZE];
        uint32_t buf32[HF2_BUF_SIZE / 4];
        uint16_t buf16[HF2_BUF_SIZE / 2];
        HF2_Command cmd;
        HF2_Response resp;
    };
} HF2_Buffer;

static bool hf2_jd_enabled;
static HF2_Buffer hf2_pkt;

static const char *uf2_info() {
    return app_dev_class_name;
}

static int hf2_send_buffer(uint8_t flag, const void *data, unsigned size, uint32_t prepend) {
    if (!usb_is_connected())
        return 0;

    uint32_t buf[64 / 4]; // aligned

    if (prepend + 1)
        size += 4;

    unsigned blocks = 0;
    unsigned sp = 0;
    while (sp < size) {
        blocks++;
        sp += 63;
    }

    int r = 0;

    target_disable_irq();
    if (usb_write_space_available() < blocks * 64) {
        OVF_ERROR("HF2 queue full");
        r = -1;
    } else {
        while (size > 0) {
            memset(buf, 0, 64);
            int s = 63;
            if (size <= 63) {
                s = size;
                buf[0] = flag;
            } else {
                buf[0] = flag == HF2_FLAG_CMDPKT_LAST ? HF2_FLAG_CMDPKT_BODY : flag;
            }
            buf[0] |= s;
            uint8_t *dst = (uint8_t *)buf;
            dst++;
            if (prepend + 1) {
                memcpy(dst, &prepend, 4);
                prepend = -1;
                dst += 4;
                s -= 4;
                size -= 4;
            }
            memcpy(dst, data, s);
            data = (const uint8_t *)data + s;
            size -= s;

            usb_write(buf, sizeof(buf));
        }
    }
    target_enable_irq();

    return r;
}

int hf2_send_event(uint32_t evId, const void *data, int size) {
    return hf2_send_buffer(HF2_FLAG_CMDPKT_LAST, data, size, evId);
}

int hf2_send_serial(const void *data, int size, int isError) {
    return hf2_send_buffer(isError ? HF2_FLAG_SERIAL_ERR : HF2_FLAG_SERIAL_OUT, data, size, -1);
}

void hf2_send_frame(const void *ptr) {
    const jd_frame_t *frame = ptr;
    if (hf2_jd_enabled)
        hf2_send_event(HF2_EV_JDS_PACKET, frame, JD_FRAME_SIZE(frame));
}

static int hf2_send_response(int size) {
    hf2_send_buffer(HF2_FLAG_CMDPKT_LAST, hf2_pkt.buf, 4 + size, -1);
    return 0;
}

static int hf2_send_response_with_data(const void *data, int size) {
    if (size <= (int)sizeof(hf2_pkt.buf) - 4) {
        memcpy(hf2_pkt.resp.data8, data, size);
        return hf2_send_response(size);
    } else {
        hf2_send_buffer(HF2_FLAG_CMDPKT_LAST, data, size, hf2_pkt.resp.eventId);
        return 0;
    }
}

static void do_send_frame(void *f) {
    jd_send_frame_raw(f);
    jd_free(f);
}

static int hf2_handle_packet(int sz) {
    if (hf2_pkt.serial) {
        // TODO raise some event?
        return 0;
    }

    // LOGV("HF2 sz=%d CMD=%x", sz, hf2_pkt.buf32[0]);

    // one has to be careful dealing with these, as they share memory
    HF2_Command *cmd = &hf2_pkt.cmd;
    HF2_Response *resp = &hf2_pkt.resp;

    uint32_t cmdId = cmd->command_id;
    resp->tag = cmd->tag;
    resp->status16 = HF2_STATUS_OK;

    //#define checkDataSize(str, add) assert(sz == 8 + (int)sizeof(cmd->str) + (int)(add))

    // lastExchange = current_time_ms();
    // gotSomePacket = true;

    switch (cmdId) {
    case HF2_CMD_INFO:
        return hf2_send_response_with_data(uf2_info(), strlen(uf2_info()));

    case HF2_CMD_BININFO:
        resp->bininfo.mode = HF2_MODE_USERSPACE;
        resp->bininfo.flash_page_size = 0;
        resp->bininfo.flash_num_pages = 0;
        resp->bininfo.max_message_size = sizeof(hf2_pkt.buf);
        resp->bininfo.uf2_family = 0xbfdd4eee;
        return hf2_send_response(sizeof(resp->bininfo));

    case HF2_CMD_RESET_INTO_APP:
        target_reset();
        break;

    case HF2_CMD_RESET_INTO_BOOTLOADER:
        reboot_to_uf2();
        break;

    case HF2_CMD_DMESG:
        // TODO
        break;

    case HF2_CMD_JDS_CONFIG:
        if (cmd->data8[0]) {
            hf2_jd_enabled = 1;
        } else {
            hf2_jd_enabled = 0;
        }
        return hf2_send_response(0);

    case HF2_CMD_JDS_SEND: {
        jd_send_frame_raw((jd_frame_t *)cmd->data8);
        return hf2_send_response(0);
    }

    default:
        // command not understood
        resp->status16 = HF2_STATUS_INVALID_CMD;
        break;
    }

    return hf2_send_response(0);
}

void hf2_recv(uint8_t buf[64]) {
    uint8_t tag = buf[0];
    if (hf2_pkt.size && (tag & HF2_FLAG_SERIAL_OUT)) {
        ERROR("serial in middle of cmd");
        return;
    }

    int size = tag & HF2_SIZE_MASK;
    if (hf2_pkt.size + size > (int)sizeof(hf2_pkt.buf)) {
        ERROR("hf2_pkt too large");
        return;
    }

    memcpy(hf2_pkt.buf + hf2_pkt.size, buf + 1, size);
    hf2_pkt.size += size;
    tag &= HF2_FLAG_MASK;
    if (tag != HF2_FLAG_CMDPKT_BODY) {
        if (tag == HF2_FLAG_CMDPKT_LAST)
            hf2_pkt.serial = 0;
        else if (tag == HF2_FLAG_SERIAL_OUT)
            hf2_pkt.serial = 1;
        else
            hf2_pkt.serial = 2;
        int sz = hf2_pkt.size;
        hf2_pkt.size = 0;
        hf2_handle_packet(sz);
    }
}
