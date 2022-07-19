#include "jdpico.h"
#include "tusb.h"
#include "interfaces/jd_hid.h"

#define LOG(msg, ...) DMESG("hid: " msg, ##__VA_ARGS__)
// #define LOGV(msg, ...) DMESG("hid: " msg, ##__VA_ARGS__)
#define LOGV JD_NOLOG
#define CHK JD_CHK

static jd_hid_keyboard_report_t kbd_report;
static bool kbd_report_dirty;

static jd_hid_mouse_report_t mouse_report;
static bool mouse_report_dirty;

static jd_hid_gamepad_report_t gamepad_report;
static bool gamepad_report_dirty;

void jd_hid_keyboard_set_report(const jd_hid_keyboard_report_t *report) {
    if (report == NULL)
        memset(&kbd_report, 0, sizeof(kbd_report));
    else
        kbd_report = *report;
    kbd_report_dirty = 1;
}

void jd_hid_mouse_move(const jd_hid_mouse_report_t *report) {
    mouse_report.buttons |= report->buttons;
    mouse_report.x += report->x;
    mouse_report.y += report->y;
    mouse_report.wheel += report->wheel;
    mouse_report.pan += report->pan;
    mouse_report_dirty = 1;
}

void jd_hid_gamepad_set_report(const jd_hid_gamepad_report_t *report) {
    if (report == NULL)
        memset(&gamepad_report, 0, sizeof(gamepad_report));
    else
        gamepad_report = *report;
    gamepad_report_dirty = 1;
}

void hid_process(void) {
#if 0
    if (kbd_report_dirty && tud_suspended())
        tud_remote_wakeup();
#endif

    if (kbd_report_dirty && tud_hid_n_ready(HID_ITF_KEYBOARD)) {
        kbd_report_dirty = 0;
        tud_hid_n_keyboard_report(HID_ITF_KEYBOARD, 0, kbd_report.modifier, kbd_report.keycode);
    }

    if (mouse_report_dirty && tud_hid_n_ready(HID_ITF_MOUSE)) {
        mouse_report_dirty = 0;
        tud_hid_n_mouse_report(HID_ITF_MOUSE, 0, mouse_report.buttons, mouse_report.x,
                               mouse_report.y, mouse_report.wheel, mouse_report.pan);
        memset(&mouse_report, 0, sizeof(mouse_report));
    }

    if (gamepad_report_dirty && tud_hid_n_ready(HID_ITF_GAMEPAD)) {
        gamepad_report_dirty = 0;
        tud_hid_n_report(HID_ITF_GAMEPAD, 0, &gamepad_report, sizeof(gamepad_report));
    }
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                               uint8_t *buffer, uint16_t reqlen) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;

    LOG("GET REPORT %d/%d", instance, report_type);

    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type,
                           uint8_t const *buffer, uint16_t bufsize) {
    (void)report_id;

    if (instance == HID_ITF_KEYBOARD) {
        if (report_type == HID_REPORT_TYPE_OUTPUT) {
            if (bufsize < 1)
                return;
            uint8_t kbd_leds = buffer[0];
            if (kbd_leds & KEYBOARD_LED_CAPSLOCK) {
                // caps on
            } else {
                // caps off
            }
        }
    }
}
