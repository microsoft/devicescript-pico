#include "jdpico.h"

#include "hardware/dma.h"
#include "hardware/irq.h"


__force_inline void irq_set_mask_enabled_(uint32_t mask, bool enabled) {
    if (enabled) {
        // Clear pending before enable
        // (if IRQ is actually asserted, it will immediately re-pend)
        *((io_rw_32 *) (PPB_BASE + M0PLUS_NVIC_ICPR_OFFSET)) = mask;
        *((io_rw_32 *) (PPB_BASE + M0PLUS_NVIC_ISER_OFFSET)) = mask;
    } else {
        *((io_rw_32 *) (PPB_BASE + M0PLUS_NVIC_ICER_OFFSET)) = mask;
    }
}

REAL_TIME_FUNC
void ram_irq_set_enabled(uint num, bool enabled) {
    irq_set_mask_enabled_(1u << num, enabled);
}


typedef struct {
    void *context;
    cb_arg_t handler;
} DMAHandler;

static DMAHandler dmaHandler[12]; // max channel 12

REAL_TIME_FUNC
void isr_dma_0() {
    uint mask = dma_hw->ints0;
    // write to clear
    dma_hw->ints0 = mask;
    for (uint8_t ch = 0; ch < 12; ch++) {
        if ((1 << ch) & mask) {
            if (dmaHandler[ch].handler) {
                dma_channel_set_irq0_enabled(ch, false);
                dmaHandler[ch].handler(dmaHandler[ch].context);
            }
        }
    }
}

REAL_TIME_FUNC
void dma_set_ch_cb(uint8_t channel, cb_arg_t handler, void *context) {
    if (channel >= 12)
        return;
    dmaHandler[channel].handler = handler;
    dmaHandler[channel].context = context;
    dma_channel_set_irq0_enabled(channel, true);
    ram_irq_set_enabled(DMA_IRQ_0, true);
}
