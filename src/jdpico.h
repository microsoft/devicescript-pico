#pragma once
#include "jd_protocol.h"
#include "pico/stdlib.h"
#include "RP2040.h"

bool jd_rx_has_frame(void);
void init_jacscript_manager(void);
void hf2_init(void);

void reboot_to_uf2(void);
void flush_dmesg(void);

void init_sdcard(void);


void platform_init(void);

// without #, GCC now appends "aw",@progbits'
// with the #, GCC comments out this appended directive
// see: https://gcc.gnu.org/legacy-ml/gcc-help/2010-09/msg00088.html
#define RAM_FUNC __attribute__((noinline, long_call, section(".data#")))
