#pragma once

#define SPI_BUF_SIZE 1024U
uint8_t spi_buf_rx[SPI_BUF_SIZE];
uint8_t spi_buf_tx[SPI_BUF_SIZE];

#define SPI_CHECKSUM_START 0xABU
#define SPI_SYNC_BYTE 0x5AU
#define SPI_HACK 0x79U
#define SPI_DACK 0x85U
#define SPI_NACK 0x1FU

// SPI states
enum {
  SPI_RX_STATE_HEADER=0U,
  SPI_RX_STATE_HEADER_ACK=1U,
  SPI_RX_STATE_HEADER_NACK=2U,
  SPI_RX_STATE_DATA_RX=3U,
  SPI_RX_STATE_DATA_RX_ACK=4U,
  SPI_RX_STATE_DATA_TX=5U
};

uint8_t spi_state = SPI_RX_STATE_HEADER;
uint8_t spi_endpoint;
uint16_t spi_data_len_mosi;
uint16_t spi_data_len_miso;

#define SPI_HEADER_SIZE 7U

bool check_checksum(uint8_t *data, uint16_t len) {
  // TODO: can speed this up by casting the bulk to uint32_t and xor-ing the bytes afterwards
  uint8_t checksum = SPI_CHECKSUM_START;
  for(uint16_t i = 0U; i < len; i++){
    checksum ^= data[i];
  }
  return checksum == 0U;
}
