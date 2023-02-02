#include "jdpico.h"
#include "hardware/flash.h"

static devsmgr_cfg_t cfg;
static uint8_t flash_page_buf[FLASH_PAGE_SIZE];

#define SPI_FLASH_SIZE 1024 // k
#define MAX_PROG_SIZE 128   // k
#define STORAGE_OFFSET ((SPI_FLASH_SIZE - MAX_PROG_SIZE) * 1024)
#define PAGE_MASK (FLASH_PAGE_SIZE - 1)

static void flash_program_page(uintptr_t addr, const uint8_t *src, uint32_t len) {
    JD_ASSERT(len < FLASH_PAGE_SIZE);
    JD_ASSERT(addr / FLASH_PAGE_SIZE == (addr + len - 1) / FLASH_PAGE_SIZE);

    uint8_t *dst = (void *)(XIP_BASE + (addr & ~PAGE_MASK));
    memcpy(flash_page_buf, dst, FLASH_PAGE_SIZE);
    uint dp = addr & PAGE_MASK;
    for (uint i = 0; i < len; ++i) {
        JD_ASSERT(dst[dp + i] == 0xff);
        flash_page_buf[dp + i] = src[i];
    }
    target_disable_irq();
    flash_range_program(addr & ~PAGE_MASK, flash_page_buf, FLASH_PAGE_SIZE);
    target_enable_irq();
}

void flash_program(void *dst, const void *src, uint32_t len) {
    JD_ASSERT(cfg.program_base != NULL);
    ptrdiff_t diff = (uintptr_t)dst - XIP_BASE - STORAGE_OFFSET;
    JD_ASSERT(0 <= diff && diff <= cfg.max_program_size - JD_FLASH_PAGE_SIZE);
    JD_ASSERT((diff & (8 - 1)) == 0);

    uintptr_t off = diff + STORAGE_OFFSET;
    while (len > 0) {
        uint n = FLASH_PAGE_SIZE - (off & PAGE_MASK);
        if (n > len)
            n = len;
        flash_program_page(off, src, n);
        off += n;
        src = (const uint8_t *)src + n;
        len -= n;
    }
}

void flash_erase(void *page_addr) {
    JD_ASSERT(cfg.program_base != NULL);
    ptrdiff_t diff = (uintptr_t)page_addr - XIP_BASE - STORAGE_OFFSET;
    JD_ASSERT(0 <= diff && diff <= cfg.max_program_size - JD_FLASH_PAGE_SIZE);
    JD_ASSERT((diff & (JD_FLASH_PAGE_SIZE - 1)) == 0);

    target_disable_irq();
    flash_range_erase(diff + STORAGE_OFFSET, JD_FLASH_PAGE_SIZE);
    target_enable_irq();
}

void init_devicescript_manager(void) {
    cfg.max_program_size = MAX_PROG_SIZE * 1024;
    cfg.program_base = (void *)(XIP_BASE + STORAGE_OFFSET);
    JD_ASSERT((STORAGE_OFFSET & (JD_FLASH_PAGE_SIZE - 1)) == 0);
    devsmgr_init(&cfg);
    devsdbg_init();
}

void flash_sync(void) {}