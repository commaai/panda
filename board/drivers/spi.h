#pragma once

#include "board/drivers/drivers.h"

// DMA2 uses SRAM1/SRAM2. Each buffer holds a reply followed by the next request.
#ifdef STM32H7
__attribute__((section(".sram12"))) uint8_t spi_buf_rx[SPI_BUF_SIZE];
__attribute__((section(".sram12"))) uint8_t spi_buf_tx[SPI_BUF_SIZE];
#else
uint8_t spi_buf_rx[SPI_BUF_SIZE];
uint8_t spi_buf_tx[SPI_BUF_SIZE];
#endif

#define SPI_SYNC_BYTE 0x5AU
#define SPI_ACK 0x85U
#define SPI_NACK 0x1FU
#define SPI_BUSY 0x79U
#define SPI_HEADER_SIZE 6U
#define SPI_VERSION_ENDPOINT 0xFFU
#define SPI_PROTOCOL_VERSION 4U

uint16_t spi_error_count = 0U;
static bool spi_can_tx_ready = false;

void llspi_init(void);
void llspi_dma(bool reply);

static bool spi_is_poll(const uint8_t *data, uint16_t len) {
  bool empty = true;
  for (uint16_t i = 0U; i < len; i++) {
    if (data[i] != 0U) {
      empty = false;
      break;
    }
  }
  return empty;
}

static uint16_t spi_version_packet(uint8_t *out) {
  (void)memcpy(out, (const uint8_t *)UID_BASE, 12U);
  out[12] = hw_type;
  out[13] = USB_PID & 0xFFU;
  out[14] = SPI_PROTOCOL_VERSION;
  return 15U;
}

void spi_init(void) {
  llspi_init();
  llspi_dma(false);
}

void spi_rx_done(const uint8_t *data) {
  (void)memset(spi_buf_tx, 0, SPI_FRAME_SIZE);
  // Request: sync, endpoint, TX length, RX capacity, payload, padding, CRC8.
  // Reply: status, RX length, payload, padding, CRC8. Both are 256 bytes.
  uint8_t status = SPI_NACK;
  uint16_t response_len = 0U;
  uint16_t rx_capacity = 0U;
  bool valid = data[0] == SPI_SYNC_BYTE;
  if (valid) {
    valid = crc_checksum(data, SPI_FRAME_SIZE - 1U, 0xD5U) == data[SPI_FRAME_SIZE - 1U];
  }
  if (valid) {
    uint8_t endpoint = data[1];
    uint16_t tx_len = (data[3] << 8) | data[2];
    rx_capacity = (data[5] << 8) | data[4];
    const uint8_t *payload = &data[SPI_HEADER_SIZE];
    valid = (tx_len <= SPI_MAX_PAYLOAD) && (rx_capacity <= SPI_MAX_PAYLOAD);
    if (valid) {
      if ((endpoint == SPI_VERSION_ENDPOINT) && (tx_len == 0U)) {
        response_len = spi_version_packet(&spi_buf_tx[3]);
        status = SPI_ACK;
      } else if ((endpoint == 0U) && (tx_len == sizeof(ControlPacket_t))) {
        ControlPacket_t ctrl = {0};
        (void)memcpy((uint8_t *)&ctrl, payload, sizeof(ctrl));
        response_len = comms_control_handler(&ctrl, &spi_buf_tx[3]);
        status = SPI_ACK;
      } else if (((endpoint == 1U) || (endpoint == 0x81U)) && (tx_len == 0U)) {
        response_len = comms_can_read(&spi_buf_tx[3], rx_capacity);
        status = SPI_ACK;
      } else if ((endpoint == 2U) && ((tx_len % 4U) == 0U)) {
        comms_endpoint2_write(payload, tx_len);
        status = SPI_ACK;
      } else if ((endpoint == 3U) && (tx_len > 0U)) {
        status = SPI_BUSY;
        if (spi_can_tx_ready) {
          spi_can_tx_ready = false;
          comms_can_write(payload, tx_len);
          status = SPI_ACK;
        }
      } else if ((endpoint == 0xABU) && (tx_len == 0U)) {
        response_len = rx_capacity;
        status = SPI_ACK;
      } else {
        // Unknown endpoints and invalid endpoint arguments have no effects.
      }
    }
  }
  if (!valid && !spi_is_poll(data, SPI_FRAME_SIZE)) {
    // Empty polls can arrive while idle during recovery.
    spi_error_count += 1U;
  }
  if (response_len > SPI_MAX_PAYLOAD) {
    response_len = 0U;
    status = SPI_NACK;
    spi_error_count += 1U;
  }
  if (response_len > rx_capacity) {
    response_len = rx_capacity;
  }
  (void)memset(&spi_buf_tx[response_len + 3U], 0, SPI_FRAME_SIZE - response_len - 4U);
  spi_buf_tx[0] = status;
  spi_buf_tx[1] = response_len & 0xFFU;
  spi_buf_tx[2] = (response_len >> 8) & 0xFFU;
  spi_buf_tx[SPI_FRAME_SIZE - 1U] = crc_checksum(spi_buf_tx, SPI_FRAME_SIZE - 1U, 0xD5U);
  llspi_dma(true);
}

void can_tx_comms_resume_spi(void) {
  spi_can_tx_ready = true;
}
