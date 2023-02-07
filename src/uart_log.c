#include "jdpico.h"

#include "hardware/dma.h"
#include "hardware/claim.h"
#include "hardware/uart.h"

#include "pico/stdio.h"
#include "pico/stdio/driver.h"

static int dmachTx = -1;
static bool in_tx;

static void stdio_uart_out_chars(const char *buf, int length) {
    jd_dmesg_write(buf, length);
}

static int stdio_uart_in_chars(char *buf, int length) {
    return PICO_ERROR_NO_DATA;
}

stdio_driver_t stdio_dmesg = {
    .out_chars = stdio_uart_out_chars,
    .in_chars = stdio_uart_in_chars,
};

void uart_log_init() {
    dmachTx = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dmachTx);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_dreq(&c, uart_get_dreq(uart_default, true));

    dma_channel_configure(dmachTx, &c, &uart_get_hw(uart_default)->dr, NULL, 0, false);

    uart_init(uart_default, PICO_DEFAULT_UART_BAUD_RATE);
    gpio_set_function(PICO_DEFAULT_UART_TX_PIN, GPIO_FUNC_UART);

    stdio_set_driver_enabled(&stdio_dmesg, true);
}

bool uart_log_can_write(void) {
    return !in_tx;
}

static void tx_handler(void *p) {
    in_tx = 0;
}

void uart_log_write(const void *buf, unsigned size) {
    if (dmachTx == -1)
        uart_log_init();
    JD_ASSERT(!in_tx);
    in_tx = 1;
    dma_set_ch_cb(dmachTx, tx_handler, NULL);
    dma_channel_transfer_from_buffer_now(dmachTx, buf, size);
}
