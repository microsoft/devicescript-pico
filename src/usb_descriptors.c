#include "jdpico.h"
#include "tusb.h"

// #define USB_VID 0x2E8A // Raspberry Pi
#define USB_VID 12346  // pretend to be ESP32 for website for now
#define USB_PID 0x000a // Raspberry Pi Pico SDK CDC

#define USB_DESC_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_DESC_LEN * HID_ITF_TOTAL)
#define USB_MAX_POWER_MA 500

#define USB_ITF_CDC 0
#define USB_ITF_MAX (HID_ITF_OFF + HID_ITF_TOTAL)

#define USB_CDC_EP_CMD 0x81
#define USB_CDC_EP_OUT 0x02
#define USB_CDC_EP_IN 0x82
#define USB_CDC_CMD_MAX_SIZE 8
#define USB_CDC_IN_OUT_MAX_SIZE 64

#define HID_EPNUM_OFF 0x83 // 0x84 and 0x85 as well

#define USB_STR_0 0x00
#define USB_STR_MANUF 0x01
#define USB_STR_PRODUCT 0x02
#define USB_STR_SERIAL 0x03
#define USB_STR_CDC 0x04
#define USB_STR_HID 0x05 // 0x06, 0x07 as well

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

static const uint8_t desc_hid_KEYBOARD[] = {TUD_HID_REPORT_DESC_KEYBOARD()};
static const uint8_t desc_hid_MOUSE[] = {TUD_HID_REPORT_DESC_MOUSE()};
static const uint8_t desc_hid_GAMEPAD[] = {
    // https://github.com/lancaster-university/codal-core/blob/master/source/drivers/HIDJoystick.cpp
    0x05, 0x01, // USAGE_PAGE (Generic Desktop)
    0x09, 0x05, // USAGE (Game Pad)
    0xa1, 0x01, // COLLECTION (Application)
    0x05, 0x02, // USAGE_PAGE (Simulation Controls)
    0x09, 0xbb, // USAGE (Throttle)
    0x15, 0x00, // LOGICAL_MINIMUM (0)
    0x25, 0x1f, // LOGICAL_MAXIMUM (31)
    0x75, 0x08, // REPORT_SIZE (8)
    0x95, 0x01, // REPORT_COUNT (1)
    0x81, 0x02, // INPUT (Data,Var,Abs)
    0x05, 0x02, // USAGE_PAGE (Simulation Controls)
    0x09, 0xb0, // USAGE (Rudder)
    0x15, 0x00, // LOGICAL_MINIMUM (0)
    0x25, 0x1f, // LOGICAL_MAXIMUM (31)
    0x75, 0x08, // REPORT_SIZE (8)
    0x95, 0x01, // REPORT_COUNT (1)
    0x81, 0x02, // INPUT (Data,Var,Abs)
    0x05, 0x01, // USAGE_PAGE (Generic Desktop)
    0xa1, 0x00, // COLLECTION (Physical)
    0x09, 0x30, // USAGE (X)
    0x09, 0x31, // USAGE (Y)
    0x09, 0x32, // USAGE (Z)
    0x09, 0x35, // USAGE (Rz)
    0x15, 0x81, // LOGICAL_MINIMUM (-127)
    0x25, 0x7f, // LOGICAL_MAXIMUM (127)
    0x75, 0x08, // REPORT_SIZE (8)
    0x95, 0x04, // REPORT_COUNT (4)
    0x81, 0x02, // INPUT (Data,Var,Abs)
    0x05, 0x09, // USAGE_PAGE (Button)
    0x19, 0x01, // USAGE_MINIMUM (Button 1)
    0x29, 0x10, // USAGE_MAXIMUM (Button 16)
    0x15, 0x00, // LOGICAL_MINIMUM (0)
    0x25, 0x01, // LOGICAL_MAXIMUM (1)
    0x75, 0x01, // REPORT_SIZE (1)
    0x95, 0x10, // REPORT_COUNT (16)
    0x81, 0x02, // INPUT (Data,Var,Abs)
    0xc0,       // END_COLLECTION
    0xc0        // END_COLLECTION
};
static const uint8_t *hid_descs[HID_ITF_TOTAL] = {
    [HID_ITF_KEYBOARD] = desc_hid_KEYBOARD,
    [HID_ITF_MOUSE] = desc_hid_MOUSE,
    [HID_ITF_GAMEPAD] = desc_hid_GAMEPAD,
};

// Interface number, string index, protocol, report descriptor len, EP In address, size &
// polling interval
#define HID_DESC(id, proto)                                                                        \
    TUD_HID_DESCRIPTOR(HID_ITF_OFF + HID_ITF_##id, USB_STR_HID + HID_ITF_##id, proto,              \
                       sizeof(desc_hid_##id), HID_EPNUM_OFF + HID_ITF_##id,                        \
                       CFG_TUD_HID_EP_BUFSIZE, 10)

static const uint8_t desc_cfg[USB_DESC_LEN] = {
    TUD_CONFIG_DESCRIPTOR(1, USB_ITF_MAX, USB_STR_0, USB_DESC_LEN, 0, USB_MAX_POWER_MA),
    TUD_CDC_DESCRIPTOR(USB_ITF_CDC, USB_STR_CDC, USB_CDC_EP_CMD, USB_CDC_CMD_MAX_SIZE,
                       USB_CDC_EP_OUT, USB_CDC_EP_IN, USB_CDC_IN_OUT_MAX_SIZE),
    HID_DESC(GAMEPAD, HID_ITF_PROTOCOL_NONE),
    HID_DESC(KEYBOARD, HID_ITF_PROTOCOL_KEYBOARD),
    HID_DESC(MOUSE, HID_ITF_PROTOCOL_MOUSE),
};

static char serial_str[2 + 2 * 8 + 1];
static const char *const string_descriptors[] = {
    [USB_STR_MANUF] = "Raspberry Pi",
    [USB_STR_PRODUCT] = "RP2040",
    [USB_STR_SERIAL] = serial_str,
    [USB_STR_CDC] = "Jacdac CDC",
    [USB_STR_HID + HID_ITF_KEYBOARD] = "Jacdac Keyboard",
    [USB_STR_HID + HID_ITF_MOUSE] = "Jacdac Mouse",
    [USB_STR_HID + HID_ITF_GAMEPAD] = "Jacdac Gamepad",
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
        if (str == NULL)
            return NULL;
        for (len = 0; len < (sizeof(desc_str) / 2) - 1 && str[len]; len++) {
            desc_str[1 + len] = str[len];
        }
    }

    // first byte is length (including header), second byte is string type
    desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * len + 2));

    return desc_str;
}

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) {
    if (instance < HID_ITF_TOTAL)
        return hid_descs[instance];
    return NULL;
}
