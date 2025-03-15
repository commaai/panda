#pragma once

#if defined(ENABLE_SPI) || defined(BOOTSTUB)
void llspi_miso_dma(uint8_t *addr, int len);

void llspi_mosi_dma(uint8_t *addr, int len);

// ***************************** SPI init *****************************
void llspi_init(void);
#endif
