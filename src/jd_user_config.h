#ifndef JD_USER_CONFIG_H
#define JD_USER_CONFIG_H

#define DEVICE_DMESG_BUFFER_SIZE 4096

#include "dmesg.h"

#define BRAIN_ID_MSR124 124
#define BRAIN_ID_MSR59 59
#define BRAIN_ID_PICO_W 1

#ifndef BRAIN_ID
#error "define BRAIN_ID"
#endif

#define JD_LOG DMESG
#define JD_WR_OVERHEAD 28

#define JD_CLIENT 1

#define LED_R_MULT 250
#define LED_G_MULT 60
#define LED_B_MULT 150

#if BRAIN_ID == BRAIN_ID_MSR124
#define PIN_LED_R 16
#define PIN_LED_G 14
#define PIN_LED_B 15
#define PWR_CFG                                                                                    \
    .pin_fault = 12, .pin_en = 22, .pin_pulse = 8, .en_active_high = 3, .fault_ignore_ms = 1000,
#define PIN_PWR_LED_PULSE 13
#define PIN_PWR_DET 11
#define PIN_PWR_N_ILIM_HI 18
#define DEV_CLASS_NAME "RP2040 DeviceScript 59 v0.1"
#define DEV_CLASS 0x35a678a3
#endif

#if BRAIN_ID == BRAIN_ID_MSR59
#define PIN_LED_R 11
#define PIN_LED_G 13
#define PIN_LED_B 15
#define PWR_CFG                                                                                    \
    .pin_fault = 25, .pin_en = 19, .pin_pulse = NO_PIN, .en_active_high = 2, .fault_ignore_ms = 100,
#define DEV_CLASS_NAME "RP2040 DeviceScript 124 v0.1"
#define DEV_CLASS 0x3875e80d
#endif

#if BRAIN_ID == BRAIN_ID_PICO_W
#define DEV_CLASS_NAME "Raspberry Pi Pico W"
#define DEV_CLASS 0x3a513204
#define JD_WIFI 1
void pico_w_set_led(uint8_t r, uint8_t g, uint8_t b);
#define LED_SET_RGB pico_w_set_led
#endif

#ifndef DEV_CLASS_NAME
#error "invalid BRAIN_ID"
#endif

#define PIN_JACDAC 9
#define PIN_SD_MISO 2
#define PIN_SD_MOSI 3
#define PIN_SD_SCK 4
#define PIN_SD_CS 5

#define JD_RAW_FRAME 1

// this is min. erase size
#define JD_FLASH_PAGE_SIZE 4096

// probably not so useful on brains...
#define JD_CONFIG_WATCHDOG 0

#define JD_USB_BRIDGE 1

#define JD_SEND_FRAME_SIZE 1024

#define JD_LSTORE 0

#define JD_SETTINGS_LARGE 1

#define JD_FSTOR_TOTAL_SIZE (128 * 1024)
#define SPI_FLASH_MEGS 1

#define SPI_STORAGE_SIZE JD_FSTOR_TOTAL_SIZE
#define SPI_STORAGE_OFFSET (SPI_FLASH_MEGS * 1024 * 1024 - SPI_STORAGE_SIZE)
#define JD_FSTOR_BASE_ADDR (0x10000000 + SPI_STORAGE_OFFSET)

#endif
