#include "jdpico.h"
#include "hardware/flash.h"

static uint32_t last_page_addr;
static uint8_t flash_page_buf[FLASH_PAGE_SIZE];

#define PAGE_MASK (FLASH_PAGE_SIZE - 1)

void flash_sync(void) {
    if (last_page_addr) {
        target_disable_irq();
        flash_range_program(last_page_addr, flash_page_buf, FLASH_PAGE_SIZE);
        target_enable_irq();
        last_page_addr = 0;
    }
}

static void flash_program_page(uintptr_t addr, const uint8_t *src, uint32_t len) {
    JD_ASSERT(len < FLASH_PAGE_SIZE);
    JD_ASSERT(addr / FLASH_PAGE_SIZE == (addr + len - 1) / FLASH_PAGE_SIZE);

    uintptr_t page_addr = addr & ~PAGE_MASK;
    uint8_t *dst = (void *)(XIP_BASE + page_addr);

    if (last_page_addr != page_addr) {
        flash_sync();
        last_page_addr = page_addr;
        memcpy(flash_page_buf, dst, FLASH_PAGE_SIZE);
    }

    uint dp = addr & PAGE_MASK;
    for (uint i = 0; i < len; ++i) {
        JD_ASSERT(dst[dp + i] == 0xff);
        flash_page_buf[dp + i] = src[i];
    }
}

void flash_program(void *dst, const void *src, uint32_t len) {
    ptrdiff_t diff = (uintptr_t)dst - XIP_BASE - SPI_STORAGE_OFFSET;
    JD_ASSERT(0 <= diff && diff + len <= SPI_STORAGE_SIZE);
    JD_ASSERT((diff & (8 - 1)) == 0);

    uintptr_t off = diff + SPI_STORAGE_OFFSET;
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
    ptrdiff_t diff = (uintptr_t)page_addr - XIP_BASE - SPI_STORAGE_OFFSET;
    JD_ASSERT(0 <= diff && diff <= SPI_STORAGE_SIZE - JD_FLASH_PAGE_SIZE);
    JD_ASSERT((diff & (JD_FLASH_PAGE_SIZE - 1)) == 0);

    target_disable_irq();
    flash_range_erase(diff + SPI_STORAGE_OFFSET, JD_FLASH_PAGE_SIZE);
    target_enable_irq();
}
