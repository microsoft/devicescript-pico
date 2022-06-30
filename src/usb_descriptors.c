#include "jdpico.h"
#include "tusb.h"

// #define USB_VID 0x2E8A // Raspberry Pi
#define USB_VID 12346  // pretend to be ESP32 for website for now
#define USB_PID 0x000a // Raspberry Pi Pico SDK CDC

#define USB_DESC_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)
#define USB_MAX_POWER_MA 250

#define USB_ITF_CDC 0
#define USB_ITF_MAX 2 // CDC needs 2 interfaces

#define USB_CDC_EP_CMD 0x81
#define USB_CDC_EP_OUT 0x02
#define USB_CDC_EP_IN 0x82
#define USB_CDC_CMD_MAX_SIZE 8
#define USB_CDC_IN_OUT_MAX_SIZE 64

#define USB_STR_0 0x00
#define USB_STR_MANUF 0x01
#define USB_STR_PRODUCT 0x02
#define USB_STR_SERIAL 0x03
#define USB_STR_CDC 0x04

static const tusb_desc_device_t desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USB_VID,
    .idProduct = USB_PID,
    .bcdDevice = 0x0100,
    .iManufacturer = USB_STR_MANUF,
    .iProduct = USB_STR_PRODUCT,
    .iSerialNumber = USB_STR_SERIAL,
    .bNumConfigurations = 1,
};

static const uint8_t desc_cfg[USB_DESC_LEN] = {
    TUD_CONFIG_DESCRIPTOR(1, USB_ITF_MAX, USB_STR_0, USB_DESC_LEN, 0, USB_MAX_POWER_MA),
    TUD_CDC_DESCRIPTOR(USB_ITF_CDC, USB_STR_CDC, USB_CDC_EP_CMD, USB_CDC_CMD_MAX_SIZE,
                       USB_CDC_EP_OUT, USB_CDC_EP_IN, USB_CDC_IN_OUT_MAX_SIZE),
};

static char serial_str[2 + 2 * 8 + 1];
static const char *const string_descriptors[] = {
    [USB_STR_MANUF] = "Raspberry Pi",
    [USB_STR_PRODUCT] = "RP2040",
    [USB_STR_SERIAL] = serial_str,
    [USB_STR_CDC] = "Jacdac CDC",
};

const uint8_t *tud_descriptor_device_cb(void) {
    return (const uint8_t *)&desc_device;
}

const uint8_t *tud_descriptor_configuration_cb(__unused uint8_t index) {
    return desc_cfg;
}

const uint16_t *tud_descriptor_string_cb(uint8_t index, __unused uint16_t langid) {
    static uint16_t desc_str[32];

    // Assign the SN using the unique flash id
    if (!serial_str[0]) {
        uint64_t devid = jd_device_id();
        serial_str[0] = 'J';
        serial_str[1] = 'D';
        jd_to_hex(serial_str + 2, &devid, 8);
    }

    int len;
    if (index == 0) {
        desc_str[1] = 0x0409;
        len = 1;
    } else {
        if (index >= sizeof(string_descriptors) / sizeof(string_descriptors[0])) {
            return NULL;
        }
        const char *str = string_descriptors[index];
        for (len = 0; len < (sizeof(desc_str) / 2) - 1 && str[len]; len++) {
            desc_str[1 + len] = str[len];
        }
    }

    // first byte is length (including header), second byte is string type
    desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * len + 2));

    return desc_str;
}
