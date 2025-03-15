#pragma once

#if defined(ENABLE_SPI) || defined(BOOTSTUB)
// master -> panda DMA start
void llspi_mosi_dma(uint8_t *addr, int len);

// panda -> master DMA start
void llspi_miso_dma(uint8_t *addr, int len);

void llspi_init(void);
#endif
