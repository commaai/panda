#pragma once

#include <stdbool.h>
#include <stdint.h>

#define SPI_CHECKSUM_START 0xABU
#define SPI_SYNC_BYTE 0x5AU
#define SPI_HACK 0x79U
#define SPI_DACK 0x85U
#define SPI_NACK 0x1FU

// SPI states
enum {
  SPI_STATE_HEADER,
  SPI_STATE_HEADER_ACK,
  SPI_STATE_HEADER_NACK,
  SPI_STATE_DATA_RX,
  SPI_STATE_DATA_RX_ACK,
  SPI_STATE_DATA_TX
};

#define SPI_HEADER_SIZE 7U

// got max rate from hitting a non-existent endpoint
// in a tight loop, plus some buffer
#define SPI_IRQ_RATE  16000U

#define SPI_BUF_SIZE 4096U
extern uint8_t spi_buf_rx[SPI_BUF_SIZE];
extern uint8_t spi_buf_tx[SPI_BUF_SIZE];

extern uint16_t spi_error_count;

void can_tx_comms_resume_spi(void);
void spi_init(void);
void spi_rx_done(void);
void spi_tx_done(bool reset);

// low level SPI prototypes
void llspi_init(void);
void llspi_mosi_dma(uint8_t *addr, int len);
void llspi_miso_dma(uint8_t *addr, int len);
