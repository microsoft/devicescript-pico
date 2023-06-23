#include "jdpico.h"

#define LOG_TAG "spi"
#include "devs_logging.h"

#include "hardware/spi.h"
#include "hardware/dma.h"

#include "services/interfaces/jd_spi.h"

#define BLOCK_SIZE 4096

static bool spi_in_use;
static uint8_t dma_claimed, dma_tx, dma_rx;
static spi_inst_t *spi;
static cb_t spi_done_cb;

void jd_spi_process(void) {
    cb_t f = spi_done_cb;
    if (f) {
        spi_done_cb = NULL;
        spi_in_use = 0;
        f();
    }
}

static void spi_dma_cb(void *cb) {
    JD_ASSERT(spi_done_cb == NULL);
    spi_done_cb = cb;
}

int jd_spi_init(const jd_spi_cfg_t *cfg) {
    if (spi_in_use)
        return -100;

    uint8_t pins[3] = {cfg->sck, cfg->miso, cfg->mosi};

    int pin = NO_PIN;
    for (int i = 0; i < 3; ++i)
        if (pins[i] != NO_PIN) {
            pin = pins[i];
            break;
        }
    if (pin == NO_PIN)
        return -1;

    spi = (pin / 8) & 1 ? spi1 : spi0;

    unsigned hz = spi_init(spi, cfg->hz);
    spi_set_format(spi, 8, (cfg->mode >> 1) & 1, cfg->mode & 1, SPI_MSB_FIRST);

    for (int i = 0; i < 3; ++i)
        if (pins[i] != NO_PIN)
            gpio_set_function(pins[i], GPIO_FUNC_SPI);

    DMESG("SPI: sck=%d miso=%d mosi=%d hz=%u->%u", cfg->sck, cfg->miso, cfg->mosi,
          (unsigned)cfg->hz, hz);

    if (!dma_claimed) {
        dma_claimed = 1;
        dma_tx = dma_claim_unused_channel(true);
        dma_rx = dma_claim_unused_channel(true);
    }

    dma_channel_config c = dma_channel_get_default_config(dma_tx);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_dreq(&c, spi_get_dreq(spi, true));
    dma_channel_configure(dma_tx, &c, &spi_get_hw(spi)->dr, NULL, 0, false);

    c = dma_channel_get_default_config(dma_rx);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_dreq(&c, spi_get_dreq(spi, false));
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    dma_channel_configure(dma_rx, &c, NULL, &spi_get_hw(spi_default)->dr, 0, false);

    return 0;
}

bool jd_spi_is_ready(void) {
    return spi != NULL && !spi_in_use;
}

unsigned jd_spi_max_block_size(void) {
    return BLOCK_SIZE;
}

int jd_spi_xfer(const void *txdata, void *rxdata, unsigned numbytes, cb_t done_fn) {
    // JD_ASSERT(!target_in_irq()); this can in fact be invoked from done_fn()...
    JD_ASSERT(done_fn != NULL);

    if (!jd_spi_is_ready())
        return -1;

    if (numbytes > BLOCK_SIZE)
        return -2;

    if (numbytes == 0)
        return -3;

    JD_ASSERT(txdata || rxdata);
    unsigned start_ch = 0;

    if (txdata) {
        dma_channel_set_read_addr(dma_tx, txdata, false);
        dma_channel_set_trans_count(dma_tx, numbytes, false);
        start_ch |= 1u << dma_tx;
    }

    if (rxdata) {
        dma_channel_set_write_addr(dma_rx, rxdata, false);
        dma_channel_set_trans_count(dma_rx, numbytes, false);
        start_ch |= 1u << dma_rx;
    }

    spi_in_use = true;
    dma_set_ch_cb(rxdata ? dma_rx : dma_tx, spi_dma_cb, done_fn);
    dma_start_channel_mask(start_ch);

    return 0;
}
