#include "board/config.h"
#include "board/utils.h"
#include "board/libc.h"
#include "board/drivers/spi.h"
#include "board/main_declarations.h"
#include "board/can_comms.h"
#include "board/flasher.h"
#include "board/crc.h"

uint8_t spi_buf_rx[SPI_BUF_SIZE] __attribute__((section(".sram12")));
uint8_t spi_buf_tx[SPI_BUF_SIZE] __attribute__((section(".sram12")));
uint16_t spi_error_count = 0;
static uint8_t spi_state = SPI_STATE_HEADER;
static uint16_t spi_data_len_mosi;
static bool spi_can_tx_ready = false;
static const unsigned char version_text[] = "VERSION";

uint16_t spi_version_packet(uint8_t *out) {
  (void)memcpy(out, version_text, 7);
  uint16_t data_len = 0;
  uint16_t data_pos = 7U + 2U;
  (void)memcpy(&out[data_pos], ((uint8_t *)UID_BASE), 12);
  data_len += 12U;
  out[data_pos + data_len] = hw_type;
  data_len += 1U;
  out[data_pos + data_len] = USB_PID & 0xFFU;
  data_len += 1U;
  out[data_pos + data_len] = 0x2;
  data_len += 1U;
  out[7] = data_len & 0xFFU;
  out[8] = (data_len >> 8) & 0xFFU;
  uint16_t resp_len = data_pos + data_len;
  out[resp_len] = crc_checksum(out, resp_len, 0xD5U);
  resp_len += 1U;
  return resp_len;
}

void spi_init(void) {
  llspi_init();
  spi_state = SPI_STATE_HEADER;
  llspi_mosi_dma(spi_buf_rx, SPI_HEADER_SIZE);
}

bool validate_checksum(const uint8_t *data, uint16_t len) {
  uint8_t checksum = SPI_CHECKSUM_START;
  for(uint16_t i = 0U; i < len; i++){ checksum ^= data[i]; }
  return checksum == 0U;
}

void spi_rx_done(void) {
  uint16_t response_len = 0U;
  uint8_t next_rx_state = SPI_STATE_HEADER_NACK;
  bool checksum_valid = false;
  static uint8_t spi_endpoint;
  static uint16_t spi_data_len_miso;

  spi_endpoint = spi_buf_rx[1];
  spi_data_len_mosi = (spi_buf_rx[3] << 8) | spi_buf_rx[2];
  spi_data_len_miso = (spi_buf_rx[5] << 8) | spi_buf_rx[4];

  if (memcmp(spi_buf_rx, version_text, 7) == 0) {
    response_len = spi_version_packet(spi_buf_tx);
    next_rx_state = SPI_STATE_HEADER_NACK;
  } else if (spi_state == SPI_STATE_HEADER) {
    checksum_valid = validate_checksum(spi_buf_rx, SPI_HEADER_SIZE);
    if ((spi_buf_rx[0] == SPI_SYNC_BYTE) && checksum_valid) {
      spi_buf_tx[0] = SPI_HACK;
      next_rx_state = SPI_STATE_HEADER_ACK;
      response_len = 1U;
    } else {
      spi_buf_tx[0] = SPI_NACK;
      next_rx_state = SPI_STATE_HEADER_NACK;
      response_len = 1U;
    }
  } else if (spi_state == SPI_STATE_DATA_RX) {
    bool response_ack = false;
    checksum_valid = validate_checksum(&(spi_buf_rx[SPI_HEADER_SIZE]), spi_data_len_mosi + 1U);
    if (checksum_valid) {
      if (spi_endpoint == 0U) {
        if (spi_data_len_mosi >= sizeof(ControlPacket_t)) {
          ControlPacket_t ctrl = {0};
          (void)memcpy((uint8_t*)&ctrl, &spi_buf_rx[SPI_HEADER_SIZE], sizeof(ControlPacket_t));
          response_len = comms_control_handler(&ctrl, &spi_buf_tx[3]);
          response_ack = true;
        }
      } else if ((spi_endpoint == 1U) || (spi_endpoint == 0x81U)) {
        if (spi_data_len_mosi == 0U) {
          response_len = comms_can_read(&(spi_buf_tx[3]), spi_data_len_miso);
          response_ack = true;
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
          }
        }
      } else if (spi_endpoint == 0xABU) {
        response_len = spi_data_len_miso;
        response_ack = true;
      } else if (spi_endpoint == 0xACU) {
        response_ack = false;
      }
    }
    if (!response_ack) {
      spi_buf_tx[0] = SPI_NACK;
      next_rx_state = SPI_STATE_HEADER_NACK;
      response_len = 1U;
    } else {
      spi_buf_tx[0] = SPI_DACK;
      spi_buf_tx[1] = response_len & 0xFFU;
      spi_buf_tx[2] = (response_len >> 8) & 0xFFU;
      uint8_t checksum = SPI_CHECKSUM_START;
      for(uint16_t i = 0U; i < (response_len + 3U); i++) { checksum ^= spi_buf_tx[i]; }
      spi_buf_tx[response_len + 3U] = checksum;
      response_len += 4U;
      next_rx_state = SPI_STATE_DATA_TX;
    }
  }

  if (response_len == 0U) {
    spi_buf_tx[0] = SPI_NACK;
    next_rx_state = SPI_STATE_HEADER_NACK;
    response_len = 1U;
  }
  llspi_miso_dma(spi_buf_tx, response_len);
  spi_state = next_rx_state;
  if (!checksum_valid) { spi_error_count += 1U; }
}

void spi_tx_done(bool reset) {
  if ((spi_state == SPI_STATE_HEADER_NACK) || reset) {
    spi_state = SPI_STATE_HEADER;
    llspi_mosi_dma(spi_buf_rx, SPI_HEADER_SIZE);
  } else if (spi_state == SPI_STATE_HEADER_ACK) {
    spi_state = SPI_STATE_DATA_RX;
    llspi_mosi_dma(&spi_buf_rx[SPI_HEADER_SIZE], spi_data_len_mosi + 1U);
  } else if (spi_state == SPI_STATE_DATA_TX) {
    spi_state = SPI_STATE_HEADER;
    llspi_mosi_dma(spi_buf_rx, SPI_HEADER_SIZE);
  } else {
    spi_state = SPI_STATE_HEADER;
    llspi_mosi_dma(spi_buf_rx, SPI_HEADER_SIZE);
  }
}

void can_tx_comms_resume_spi(void) {
  spi_can_tx_ready = true;
}
