#include "jdpico.h"

#if JD_WIFI

#define LOG_TAG "sock"
#include "devs_logging.h"

#include "lwip/ip_addr.h"
#include "lwip/mem.h"
#include "lwip/err.h"
#include "lwip/init.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"
#include "lwip/timeouts.h"

typedef struct {
    ip_addr_t ipaddr;
    char *hostname;
    struct tcp_pcb *tcp_pcb;
    uint16_t port_num;
    uint16_t host_found_pending : 1;
    uint16_t connected : 1;
    uint16_t connecting : 1;
} sock_state_t;

static sock_state_t sock_state;

void jd_tcpsock_close(void) {
    sock_state_t *state = &sock_state;
    if (state->tcp_pcb != NULL) {
        tcp_arg(state->tcp_pcb, NULL);
        tcp_poll(state->tcp_pcb, NULL, 0);
        tcp_sent(state->tcp_pcb, NULL);
        tcp_recv(state->tcp_pcb, NULL);
        tcp_err(state->tcp_pcb, NULL);
        int err = tcp_close(state->tcp_pcb);
        if (err != ERR_OK) {
            LOG("close failed %d", err);
            tcp_abort(state->tcp_pcb);
        }
        state->tcp_pcb = NULL;
    }

    jd_free(state->hostname);
    memset(state, 0, sizeof(*state));
}

static void raise_error(sock_state_t *state, const char *msg, ...) {
    char *msgfmt = NULL;

    if (!state->connecting)
        return;

    if (msg) {
        va_list arg;
        va_start(arg, msg);
        msgfmt = jd_vsprintf_a(msg, arg);
        va_end(arg);
    }

    jd_tcpsock_close();

    if (msgfmt) {
        jd_tcpsock_on_event(JD_CONN_EV_ERROR, msgfmt, strlen(msgfmt));
        jd_free(msgfmt);
    }

    jd_tcpsock_on_event(JD_CONN_EV_CLOSE, NULL, 0);
}

static err_t tcp_client_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
    sock_state_t *state = (sock_state_t *)arg;
    if (err != ERR_OK) {
        raise_error(state, "connect failed: %d", err);
        return ERR_OK;
    }

    LOG("connected");
    state->connected = true;
    jd_tcpsock_on_event(JD_CONN_EV_OPEN, NULL, 0);
    return ERR_OK;
}

static void tcp_client_err(void *arg, err_t err) {
    sock_state_t *state = (sock_state_t *)arg;
    if (err != ERR_ABRT) {
        raise_error(state, "sock error: %d", err);
    }
}

static err_t tcp_client_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    sock_state_t *state = (sock_state_t *)arg;

    if (!p) {
        raise_error(state, "recv without buf?");
        return ERR_OK;
    }

    if (p->tot_len > 0) {
        unsigned len = 0;
        LOGV("recv %d err %d", p->tot_len, err);
        for (struct pbuf *q = p; q != NULL; q = q->next) {
            jd_tcpsock_on_event(JD_CONN_EV_MESSAGE, q->payload, q->len);
            len += q->len;
        }

        JD_ASSERT(len == p->tot_len);
        tcp_recved(tpcb, p->tot_len);
    }
    pbuf_free(p);

    return ERR_OK;
}

static void host_found(const char *hostname, const ip_addr_t *ipaddr, void *arg) {
    sock_state_t *state = arg;

    state->ipaddr = *ipaddr;

    LOG("connecting to %s:%d (%s)", hostname, state->port_num, ip4addr_ntoa(&state->ipaddr));

    state->tcp_pcb = tcp_new_ip_type(IP_GET_TYPE(&state->ipaddr));
    if (!state->tcp_pcb) {
        raise_error(state, "can't alloc pcb");
        return;
    }

    tcp_arg(state->tcp_pcb, state);
    tcp_recv(state->tcp_pcb, tcp_client_recv);
    tcp_err(state->tcp_pcb, tcp_client_err);

    int err = tcp_connect(state->tcp_pcb, &state->ipaddr, state->port_num, tcp_client_connected);

    if (err)
        raise_error(state, "can't start connect: %d", err);
}

int jd_tcpsock_new(const char *hostname, int port) {
    sock_state_t *state = &sock_state;

    if (!port || (unsigned)port > 0xffff)
        return -100;

    jd_tcpsock_close();

    state->port_num = port;
    state->hostname = jd_strdup(hostname);
    state->connecting = 1;

    int err = dns_gethostbyname(state->hostname, &state->ipaddr, host_found, state);

    if (err == ERR_INPROGRESS)
        return 0;

    if (err == ERR_OK) {
        state->host_found_pending = 1;
    } else {
        raise_error(state, "can't resolve '%s': %d", state->hostname, err);
        jd_tcpsock_close();
    }

    return err;
}

int jd_tcpsock_write(const void *buf, unsigned size) {
    sock_state_t *state = &sock_state;

    if (!state->connected)
        return -10;

    JD_ASSERT(size < 0xffff);

    // TCP_WRITE_FLAG_MORE (0x02) for TCP connection, PSH flag will not be set on last segment sent,
    int err = tcp_write(state->tcp_pcb, buf, size, TCP_WRITE_FLAG_COPY);

    if (err) {
        raise_error(state, "write error: %d", err);
    } else {
        err = tcp_output(state->tcp_pcb);
        if (err)
            raise_error(state, "flush error: %d", err);
    }

    return err;
}

void jd_tcpsock_process(void) {
    sock_state_t *state = &sock_state;

    if (state->host_found_pending) {
        state->host_found_pending = 1;
        host_found(state->hostname, &state->ipaddr, state);
    }
}

#endif