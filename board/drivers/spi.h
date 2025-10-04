#pragma once

#include "board/drivers/spi_declarations.h"
#include "board/crc.h"

uint8_t spi_buf_rx[SPI_BUF_SIZE];
uint8_t spi_buf_tx[SPI_BUF_SIZE];

uint16_t spi_error_count = 0;

static uint16_t spi_data_len_mosi;
static bool spi_can_tx_ready = false;
static const unsigned char version_text[] = "VERSION";

static uint16_t spi_version_packet(uint8_t *out) {
  // this protocol version request is a stable portion of
  // the panda's SPI protocol. its contents match that of the
  // panda USB descriptors and are sufficent to list/enumerate
  // a panda, determine panda type, and bootstub status.

  // the response is:
  // VERSION + 2 byte data length + data + CRC8

  // echo "VERSION"
  (void)memcpy(out, version_text, 7);

  // write response
  uint16_t data_len = 0;
  uint16_t data_pos = 7U + 2U;

  // write serial
  (void)memcpy(&out[data_pos], ((uint8_t *)UID_BASE), 12);
  data_len += 12U;

  // HW type
  out[data_pos + data_len] = hw_type;
  data_len += 1U;

  // bootstub
  out[data_pos + data_len] = USB_PID & 0xFFU;
  data_len += 1U;

  // SPI protocol version
  out[data_pos + data_len] = 0x2;
  data_len += 1U;

  // data length
  out[7] = data_len & 0xFFU;
  out[8] = (data_len >> 8) & 0xFFU;

  // CRC8
  uint16_t resp_len = data_pos + data_len;
  out[resp_len] = crc_checksum(out, resp_len, 0xD5U);
  resp_len += 1U;

  return resp_len;
}

void spi_init(void) {
  // platform init
  llspi_init();

  // start listening!
  llspi_mosi_dma(spi_buf_rx, SPI_BUF_SIZE);
}

static bool validate_checksum(const uint8_t *data, uint16_t len) {
  // TODO: can speed this up by casting the bulk to uint32_t and xor-ing the bytes afterwards
  uint8_t checksum = SPI_CHECKSUM_START;
  for(uint16_t i = 0U; i < len; i++){
    checksum ^= data[i];
  }
  return checksum == 0U;
}

void spi_rx_done(void) {
  uint16_t response_len = 0U;
  bool checksum_valid = false;
  static uint8_t spi_endpoint;
  static uint16_t spi_data_len_miso;

  if (memcmp(spi_buf_rx, version_text, 7) == 0) {
    response_len = spi_version_packet(spi_buf_tx);
  } else {
    // Parse combined header + data in a single fixed-size RX frame
    spi_endpoint = spi_buf_rx[1];
    spi_data_len_mosi = (spi_buf_rx[3] << 8) | spi_buf_rx[2];
    spi_data_len_miso = (spi_buf_rx[5] << 8) | spi_buf_rx[4];

    // Clamp lengths to available buffer space to avoid OOB
    uint16_t max_mosi = (uint16_t)(SPI_BUF_SIZE - SPI_HEADER_SIZE - 1U);
    if (spi_data_len_mosi > max_mosi) {
      spi_data_len_mosi = max_mosi;
    }
    uint16_t max_miso = (uint16_t)(SPI_BUF_SIZE - 4U);  // DACK + LEN(2) + CRC8
    if (spi_data_len_miso > max_miso) {
      spi_data_len_miso = max_miso;
    }

    // Validate header
    bool header_ok = false;
    if (spi_buf_rx[0] == SPI_SYNC_BYTE) {
      header_ok = validate_checksum(spi_buf_rx, SPI_HEADER_SIZE);
    }

    bool response_ack = false;
    if (header_ok) {
      // Validate data+checksum. Data checksum byte sits immediately after data
      checksum_valid = validate_checksum(&(spi_buf_rx[SPI_HEADER_SIZE]), spi_data_len_mosi + 1U);

      if (checksum_valid) {
        // Dispatch
        if (spi_endpoint == 0U) {
          if (spi_data_len_mosi >= sizeof(ControlPacket_t)) {
            ControlPacket_t ctrl = {0};
            (void)memcpy((uint8_t*)&ctrl, &spi_buf_rx[SPI_HEADER_SIZE], sizeof(ControlPacket_t));
            response_len = comms_control_handler(&ctrl, &spi_buf_tx[3]);
            response_ack = true;
          } else {
            print("SPI: insufficient data for control handler\n");
          }
        } else if ((spi_endpoint == 1U) || (spi_endpoint == 0x81U)) {
          if (spi_data_len_mosi == 0U) {
            response_len = comms_can_read(&(spi_buf_tx[3]), spi_data_len_miso);
            response_ack = true;
          } else {
            print("SPI: did not expect data for can_read\n");
          }
        } else if (spi_endpoint == 2U) {
          comms_endpoint2_write(&spi_buf_rx[SPI_HEADER_SIZE], spi_data_len_mosi);
          response_ack = true;
        } else if (spi_endpoint == 3U) {
          if (spi_data_len_mosi > 0U) {
            if (spi_can_tx_ready) {
              spi_can_tx_ready = false;
              comms_can_write(&spi_buf_rx[SPI_HEADER_SIZE], spi_data_len_mosi);
              response_ack = true;
            } else {
              response_ack = false;
              print("SPI: CAN NACK\n");
            }
          } else {
            print("SPI: expected data for can_write\n");
          }
        } else if (spi_endpoint == 0xABU) {
          // test endpoint: mimics panda -> device transfer
          response_len = spi_data_len_miso;
          response_ack = true;
        } else if (spi_endpoint == 0xACU) {
          // test endpoint: mimics device -> panda transfer (with NACK)
          response_ack = false;
        } else {
          print("SPI: unexpected endpoint"); puth(spi_endpoint); print("\n");
        }
      }
    }

    if (!header_ok || !checksum_valid) {
      spi_error_count += 1U;
    }

    if (!response_ack) {
      spi_buf_tx[0] = SPI_NACK;
      response_len = 1U;
    } else {
      // Enforce response length limits
      if (response_len > spi_data_len_miso) {
        response_len = spi_data_len_miso;
      }

      // Setup response header
      spi_buf_tx[0] = SPI_DACK;
      spi_buf_tx[1] = response_len & 0xFFU;
      spi_buf_tx[2] = (response_len >> 8) & 0xFFU;

      // Add checksum over header + payload
      uint8_t checksum = SPI_CHECKSUM_START;
      for (uint16_t i = 0U; i < (response_len + 3U); i++) {
        checksum ^= spi_buf_tx[i];
      }
      spi_buf_tx[response_len + 3U] = checksum;
      // response_len now refers to header + payload + checksum
      response_len += 4U;
    }
  }

  // send out the response
  llspi_miso_dma(spi_buf_tx, SPI_BUF_SIZE);
}

void spi_tx_done() {
  llspi_mosi_dma(spi_buf_rx, SPI_BUF_SIZE);
}

void can_tx_comms_resume_spi(void) {
  spi_can_tx_ready = true;
}
