#pragma once
#include <stdint.h>
#include <stdbool.h>

#if defined(ENABLE_SPI) || defined(BOOTSTUB)
void llspi_init(void);
void llspi_mosi_dma(uint8_t *addr, int len);
void llspi_miso_dma(uint8_t *addr, int len);
void spi_rx_done(void);
void spi_tx_done(bool timeout);
#endif
