#ifndef LLSPI_H
#define LLSPI_H

#include "board/drivers/spi.h"

void llspi_mosi_dma(uint8_t *addr, int len);
void llspi_miso_dma(uint8_t *addr, int len);
void llspi_init(void);

#endif
