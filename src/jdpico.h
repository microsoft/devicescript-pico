#pragma once

#include "jd_client.h"
#include "jacscript/jacscript.h"
#include "services/jd_services.h"
#include "services/interfaces/jd_pins.h"

#include "pico/stdlib.h"
#include "RP2040.h"

void init_jacscript_manager(void);
void hf2_init(void);

void reboot_to_uf2(void);
void flush_dmesg(void);

void init_sdcard(void);

void platform_init(void);

#define REAL_TIME_FUNC __attribute__((section(".time_critical.sws")))

typedef void (*cb_arg_t)(void *);
void dma_set_ch_cb(uint8_t channel, cb_arg_t handler, void *context);
void ram_irq_set_enabled(uint num, bool enabled);
void ram_irq_set_priority(uint num, uint8_t hardware_priority);

#define EXTI_FALLING 0x01
#define EXTI_RISING 0x02
void exti_set_callback(uint8_t pin, cb_t callback, uint32_t flags);
void exti_disable(uint8_t pin);
void exti_enable(uint8_t pin);

#define IRQ_PRIORITY_DEFAULT PICO_DEFAULT_IRQ_PRIORITY
#define IRQ_PRIORITY_TIM (2 << 6)
#define IRQ_PRIORITY_DMA (1 << 6)
#define IRQ_PRIORITY_EXTI (0 << 6)
#define IRQ_PRIORITY_UART (0 << 6)
