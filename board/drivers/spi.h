// SPI driver interface
#pragma once

#include "board/drivers/drivers.h"

extern uint16_t spi_error_count;

#define SPI_BUF_SIZE 4096U
#define SPI_HEADER_SIZE 7U

// SPI states
enum {
  SPI_STATE_HEADER,
  SPI_STATE_HEADER_ACK,
  SPI_STATE_HEADER_NACK,
  SPI_STATE_DATA_RX,
  SPI_STATE_DATA_RX_ACK,
  SPI_STATE_DATA_TX
};

// Low level prototypes
void llspi_init(void);
void llspi_mosi_dma(uint8_t *addr, int len);
void llspi_miso_dma(uint8_t *addr, int len);

// Public functions
void spi_init(void);
void spi_rx_done(void);
void spi_tx_done(bool reset);
void can_tx_comms_resume_spi(void);
uint16_t spi_version_packet(uint8_t *out);
