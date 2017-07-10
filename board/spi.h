#ifndef PANDA_SPI_H
#define PANDA_SPI_H

void spi_init();

void spi_tx_dma(void *addr, int len);
void spi_rx_dma(void *addr, int len);

#endif
