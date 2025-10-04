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

// Protocol constants
#define SPI_CHECKSUM_START 0xABU
#define SPI_SYNC_BYTE 0x5AU
#define SPI_DACK 0x85U
#define SPI_NACK 0x1FU

extern uint16_t spi_error_count;

// Fixed frame contains a 7-byte header followed by data and a 1-byte data checksum.
//   header: [SYNC(1) | EP(1) | MOSI_LEN(2 LE) | MISO_MAX(2 LE) | HDR_CKSUM(1)]
//   data:   MOSI_LEN bytes starting at offset SPI_HEADER_SIZE
//   data checksum byte immediately following data.
// The device responds in the next frame with:
//   [DACK(1) | RESP_LEN(2 LE) | RESP_DATA | RESP_CKSUM(1)]
#define SPI_HEADER_SIZE 7U

// low level SPI prototypes
void llspi_init(void);
void llspi_mosi_dma(uint8_t *addr, int len);
void llspi_miso_dma(uint8_t *addr, int len);

void can_tx_comms_resume_spi(void);
void spi_init(void);
void spi_rx_done(void);
void spi_tx_done(void);
