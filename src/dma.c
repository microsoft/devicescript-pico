#include "jdpico.h"

#include "hardware/dma.h"
#include "hardware/irq.h"

__force_inline void irq_set_mask_enabled_(uint32_t mask, bool enabled) {
    if (enabled) {
        // Clear pending before enable
        // (if IRQ is actually asserted, it will immediately re-pend)
        *((io_rw_32 *)(PPB_BASE + M0PLUS_NVIC_ICPR_OFFSET)) = mask;
        *((io_rw_32 *)(PPB_BASE + M0PLUS_NVIC_ISER_OFFSET)) = mask;
    } else {
        *((io_rw_32 *)(PPB_BASE + M0PLUS_NVIC_ICER_OFFSET)) = mask;
    }
}

JD_FAST
void ram_irq_set_priority(uint num, uint8_t hardware_priority) {
    // note that only 32 bit writes are supported
    io_rw_32 *p = (io_rw_32 *)((PPB_BASE + M0PLUS_NVIC_IPR0_OFFSET) + (num & ~3u));
    *p = (*p & ~(0xffu << (8 * (num & 3u)))) | (((uint32_t)hardware_priority) << (8 * (num & 3u)));
}

JD_FAST
void ram_irq_set_enabled(uint num, bool enabled) {
    irq_set_mask_enabled_(1u << num, enabled);
}

typedef struct {
    void *context;
    cb_arg_t handler;
} DMAHandler;

static DMAHandler dmaHandler[12]; // max channel 12

JD_FAST
void isr_dma_0() {
    uint mask = dma_hw->ints0;
    // write to clear
    dma_hw->ints0 = mask;
    for (uint8_t ch = 0; ch < 12; ch++) {
        if ((1 << ch) & mask) {
            cb_arg_t f = dmaHandler[ch].handler;
            if (f) {
                dma_channel_set_irq0_enabled(ch, false);
                dmaHandler[ch].handler = NULL;
                f(dmaHandler[ch].context);
            }
        }
    }
}

JD_FAST
void dma_dump(void) {
    DMESG("DMA ints0=%x inte0=%x en=%d", dma_hw->ints0, dma_hw->inte0,
          (int)irq_is_enabled(DMA_IRQ_0));
}

JD_FAST
void dma_set_ch_cb(uint8_t channel, cb_arg_t handler, void *context) {
    if (channel >= 12)
        return;
    JD_ASSERT(!dmaHandler[channel].handler);
    dmaHandler[channel].handler = handler;
    dmaHandler[channel].context = context;
    dma_hw->ints0 = (1 << channel); // clear any pending irq
    dma_channel_set_irq0_enabled(channel, true);
    ram_irq_set_enabled(DMA_IRQ_0, true);
}
