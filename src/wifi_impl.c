#include "jdpico.h"

#if JD_WIFI

#include "pico/cyw43_arch.h"

#define LOG_TAG "cyw43"
#include "devs_logging.h"

// #define LOG(msg, ...) printf(msg "\n", ##__VA_ARGS__)

static jd_wifi_results_t *scan_res;
static uint16_t scan_ptr, scan_size;
static bool inited, in_scan;

static int scan_result(void *env, const cyw43_ev_scan_result_t *result) {
    if (result && result->ssid_len && result->ssid[0]) {
        // LOG("ssid: '%s' %d %d", result->ssid , result->ssid_len, result->rssi);
        jd_wifi_results_t ent = {
            .rssi = result->rssi,
            .channel = result->channel,
            .flags = result->auth_mode & 7 ? JD_WIFI_APFLAGS_HAS_PASSWORD : 0,
        };
        memcpy(ent.ssid, result->ssid, result->ssid_len);
        memcpy(ent.bssid, result->bssid, 6);

        bool found = false;

        for (unsigned i = 0; i < scan_ptr; ++i) {
            if (memcmp(ent.bssid, scan_res[i].bssid, 6) == 0) {
                scan_res[i] = ent;
                found = true;
                break;
            }
        }

        if (!found) {
            if (scan_ptr >= scan_size) {
                scan_size = scan_size * 2 + 5;
                jd_wifi_results_t *tmp = jd_alloc(sizeof(jd_wifi_results_t) * scan_size);
                memcpy(tmp, scan_res, scan_ptr * sizeof(jd_wifi_results_t));
                jd_free(scan_res);
                scan_res = tmp;
            }

            scan_res[scan_ptr++] = ent;
        }

        // LOG("ssid: %s %d %d", ent.ssid, ent.rssi, ent.channel);
    }

    return 0;
}

int jd_wifi_start_scan(void) {
    if (cyw43_wifi_scan_active(&cyw43_state))
        return -10;

    in_scan = true;
    cyw43_wifi_scan_options_t scan_options = {0};
    return cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, scan_result);
}

int jd_wifi_connect(const char *ssid, const char *pw) {
    uint32_t auth = pw && *pw ? CYW43_AUTH_WPA2_AES_PSK : CYW43_AUTH_OPEN;
    int r = cyw43_arch_wifi_connect_async(ssid, pw, auth);
    LOG("connect '%s' -> %d", ssid, r);
    return r;
}

int jd_wifi_disconnect(void) {
    int r = cyw43_wifi_leave(&cyw43_state, CYW43_ITF_STA);
    LOG("disconnect -> %d", r);
    return r;
}

static const char *status_name(int status) {
    switch (status) {
    case CYW43_LINK_DOWN:
        return "link down";
    case CYW43_LINK_JOIN:
        return "joining";
    case CYW43_LINK_NOIP:
        return "no ip";
    case CYW43_LINK_UP:
        return "link up";
    case CYW43_LINK_FAIL:
        return "link fail";
    case CYW43_LINK_NONET:
        return "nonet";
    case CYW43_LINK_BADAUTH:
        return "bad auth";
    }
    return "unknown";
}

int jd_wifi_rssi(void) {
    // don't seem to have API for this - we fake it based on link status
    switch (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA)) {
    case CYW43_LINK_DOWN: // Wifi down
        return -128;
    case CYW43_LINK_JOIN: // Connected to wifi
        return -122;
    case CYW43_LINK_NOIP: // Connected to wifi, but no IP address
        return -121;
    case CYW43_LINK_UP: // Connect to wifi with an IP address
        return -40;
    case CYW43_LINK_FAIL: // Connection failed
        return -127;
    case CYW43_LINK_NONET: // No matching SSID found (could be out of range, or down)
        return -126;
    case CYW43_LINK_BADAUTH: // Authenticatation failure
        return -125;
    default:
        return -124;
    }
}

int jd_wifi_init(uint8_t mac_out[6]) {
    LOG("start init");
    if (cyw43_arch_init()) {
        DMESG("! can't init wifi");
        return -1;
    }
    cyw43_arch_enable_sta_mode();
    memcpy(mac_out, cyw43_state.mac, 6);
    LOG("init ok; mac: %-s", jd_to_hex_a(mac_out, 6));
    // cyw43_state.trace_flags = ~0;
    inited = true;
    return 0;
}

void jd_wifi_process(void) {
    cyw43_arch_poll();

    if (in_scan && !cyw43_wifi_scan_active(&cyw43_state)) {
        in_scan = false;
        LOG("scan done!");
        jd_wifi_scan_done_cb(scan_res, scan_ptr);
        scan_res = NULL;
        scan_ptr = 0;
        scan_size = 0;
    }

    static int prev_status = -1000;
    int new_status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
    if (prev_status != new_status) {
        LOG("status: %s", status_name(new_status));
        int was_up = prev_status == CYW43_LINK_UP;
        prev_status = new_status;
        if (new_status == CYW43_LINK_UP) {
            jd_wifi_got_ip_cb(cyw43_state.netif[CYW43_ITF_STA].ip_addr.addr);
        } else if (was_up) {
            jd_wifi_disconnect();
            jd_wifi_lost_ip_cb();
        }
    }
}

void jd_tcpsock_init(void) {}

void pico_w_set_led(uint8_t r, uint8_t g, uint8_t b) {
    if (!inited)
        return;

    static bool is_on;
    bool should_be_on = r > 0 || g > 0 || b > 0;
    if (is_on != should_be_on) {
        is_on = should_be_on;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, is_on);
    }
}

#endif