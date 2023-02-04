/*
 * Copyright (c) 2022 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "jdpico.h"

// very simplified version of https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/pico_cyw43_arch/cyw43_arch_poll.c
// the IRQ was conflicting with JD IRQs and wasn't used anyways

#include "pico/cyw43_arch.h"
#include "cyw43_stats.h"
#include <lwip/init.h>
#include "lwip/timeouts.h"

#if CYW43_LWIP && !NO_SYS
#error PICO_CYW43_ARCH_POLL requires lwIP NO_SYS=1
#endif

uint8_t cyw43_core_num;

int cyw43_arch_init(void) {
    cyw43_core_num = (uint8_t)get_core_num();
    cyw43_init(&cyw43_state);
    lwip_init();
    return 0;
}

void cyw43_arch_deinit(void) {
    JD_PANIC();
}

void cyw43_schedule_internal_poll_dispatch(__unused void (*func)(void)) {}

void cyw43_arch_poll(void) {
    CYW43_STAT_INC(LWIP_RUN_COUNT);
    sys_check_timeouts();
    if (cyw43_poll)
        cyw43_poll();
}

void cyw43_thread_check() {
    if (__get_current_exception() || get_core_num() != cyw43_core_num)
        JD_PANIC();
}