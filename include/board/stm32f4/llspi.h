#pragma once
#include "spi.h"
#include "drivers/interrupts.h"
#include "drivers/registers.h"

#if defined(ENABLE_SPI) || defined(BOOTSTUB)

// Function declarations for SPI operations
void llspi_mosi_dma(uint8_t *addr, int len);
void llspi_miso_dma(uint8_t *addr, int len);
void llspi_init(void);

#endif