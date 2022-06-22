#pragma once
#include "jd_protocol.h"
#include "pico/stdlib.h"

bool jd_rx_has_frame(void);
void init_jacscript_manager(void);
void hf2_init(void);

void reboot_to_uf2(void);
void flush_dmesg(void);

void init_sdcard(void);


void platform_init(void);