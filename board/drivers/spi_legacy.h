#pragma once

#include "board/drivers/drivers.h"

// Bootstub protocol v2: VERSION discovery, control requests, and flash writes.
__attribute__((section(".sram12"))) uint8_t spi_buf_rx[SPI_BUF_SIZE];
__attribute__((section(".sram12"))) uint8_t spi_buf_tx[SPI_BUF_SIZE];
uint16_t spi_error_count = 0U;

#define SPI_CHECKSUM_START 0xABU
#define SPI_HEADER_SIZE 7U
#define SPI_HACK 0x79U
#define SPI_DACK 0x85U
#define SPI_NACK 0x1FU

enum {
  SPI_STATE_HEADER,
  SPI_STATE_HEADER_ACK,
  SPI_STATE_DATA_RX,
  SPI_STATE_RESPONSE,
};
static uint8_t spi_state;
static uint16_t spi_data_len_mosi;

void llspi_init(void);
void llspi_mosi_dma(uint8_t *addr, int len);
void llspi_miso_dma(const uint8_t *addr, int len);

static uint8_t spi_checksum(const uint8_t *data, uint16_t len) {
  uint8_t checksum = SPI_CHECKSUM_START;
  for (uint16_t i = 0U; i < len; i++) {
    checksum ^= data[i];
  }
  return checksum;
}

static uint16_t spi_version_packet(void) {
  (void)memcpy(spi_buf_tx, "VERSION", 7U);
  spi_buf_tx[7] = 15U;
  spi_buf_tx[8] = 0U;
  (void)memcpy(&spi_buf_tx[9], (const uint8_t *)UID_BASE, 12U);
  spi_buf_tx[21] = hw_type;
  spi_buf_tx[22] = USB_PID & 0xFFU;
  spi_buf_tx[23] = 2U;
  spi_buf_tx[24] = crc_checksum(spi_buf_tx, 24U, 0xD5U);
  return 25U;
}

void spi_init(void) {
  llspi_init();
  spi_state = SPI_STATE_HEADER;
  llspi_mosi_dma(spi_buf_rx, SPI_HEADER_SIZE);
}

void spi_rx_done(void) {
  uint16_t response_len = 1U;
  uint8_t next_state = SPI_STATE_RESPONSE;
  bool valid = false;
  spi_buf_tx[0] = SPI_NACK;

  if (spi_state == SPI_STATE_HEADER) {
    if (memcmp(spi_buf_rx, "VERSION", SPI_HEADER_SIZE) == 0) {
      response_len = spi_version_packet();
      valid = true;
    } else {
      spi_data_len_mosi = ((uint16_t)spi_buf_rx[3] << 8U) | spi_buf_rx[2];
      valid = (spi_buf_rx[0] == 0x5AU) && (spi_checksum(spi_buf_rx, SPI_HEADER_SIZE) == 0U) &&
              (spi_data_len_mosi <= (SPI_BUF_SIZE - SPI_HEADER_SIZE - 1U));
      if (valid) {
        spi_buf_tx[0] = SPI_HACK;
        next_state = SPI_STATE_HEADER_ACK;
      }
    }
  } else if (spi_state == SPI_STATE_DATA_RX) {
    const uint8_t *payload = &spi_buf_rx[SPI_HEADER_SIZE];
    if (spi_checksum(payload, spi_data_len_mosi + 1U) == 0U) {
      uint16_t payload_len = 0U;
      if ((spi_buf_rx[1] == 0U) && (spi_data_len_mosi >= sizeof(ControlPacket_t))) {
        ControlPacket_t ctrl;
        (void)memcpy(&ctrl, payload, sizeof(ctrl));
        payload_len = comms_control_handler(&ctrl, &spi_buf_tx[3]);
        valid = true;
      } else if (spi_buf_rx[1] == 2U) {
        comms_endpoint2_write(payload, spi_data_len_mosi);
        valid = true;
      }
      if (valid) {
        spi_buf_tx[0] = SPI_DACK;
        spi_buf_tx[1] = payload_len & 0xFFU;
        spi_buf_tx[2] = payload_len >> 8U;
        spi_buf_tx[payload_len + 3U] = spi_checksum(spi_buf_tx, payload_len + 3U);
        response_len = payload_len + 4U;
      }
    }
  }
  if (!valid) {
    spi_error_count++;
  }
  spi_state = next_state;
  llspi_miso_dma(spi_buf_tx, response_len);
}

void spi_tx_done(bool reset) {
  if (!reset && (spi_state == SPI_STATE_HEADER_ACK)) {
    spi_state = SPI_STATE_DATA_RX;
    llspi_mosi_dma(&spi_buf_rx[SPI_HEADER_SIZE], spi_data_len_mosi + 1U);
  } else {
    spi_state = SPI_STATE_HEADER;
    llspi_mosi_dma(spi_buf_rx, SPI_HEADER_SIZE);
  }
}

void can_tx_comms_resume_spi(void) {}
