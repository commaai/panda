#pragma once

#include "board/crc.h"

#define SPI_TIMEOUT_US 10000U

// got max rate from hitting a non-existent endpoint
// in a tight loop, plus some buffer
#define SPI_IRQ_RATE  16000U

#define SPI_BUF_SIZE 4096U
// H7 DMA2 located in D2 domain, so we need to use SRAM1/SRAM2
__attribute__((section(".sram12"))) extern uint8_t spi_buf_rx[SPI_BUF_SIZE];
__attribute__((section(".sram12"))) extern uint8_t spi_buf_tx[SPI_BUF_SIZE];

#define SPI_CHECKSUM_START 0xABU
#define SPI_SYNC_BYTE 0x5AU
#define SPI_HACK 0x79U
#define SPI_DACK 0x85U
#define SPI_NACK 0x1FU

// SPI states
enum {
  SPI_STATE_HEADER = 0U,
  SPI_STATE_DATA = 1U
};

extern uint16_t spi_error_count;

#define SPI_HEADER_SIZE 7U

// low level SPI prototypes
void llspi_init(void);
void llspi_dump_state(void);
void llspi_dma(uint8_t *tx_addr, int tx_len, uint8_t *rx_addr, int rx_len);

void can_tx_comms_resume_spi(void);
void spi_init(void);
void spi_done(void);
