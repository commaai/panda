#pragma once

#include "board/drivers/drivers.h"

// H7 DMA2 located in D2 domain, so we need to use SRAM1/SRAM2
#ifdef STM32H7
__attribute__((section(".sram12"))) uint8_t spi_buf_rx[SPI_BUF_SIZE];
__attribute__((section(".sram12"))) uint8_t spi_buf_tx[SPI_BUF_SIZE];
#else
uint8_t spi_buf_rx[SPI_BUF_SIZE];
uint8_t spi_buf_tx[SPI_BUF_SIZE];
#endif

// SPI protocol, see python/spi.py for the host side.
//
// The host sends each request in one transfer, until the first byte it gets back is SPI_LISTENING:
//   MOSI: endpoint, seq, tx_len (u16), rx_len (u16), data[tx_len], crc32 (u32)
// then reads rx_len bytes at a time, until the first byte is a response:
//   MISO: SPI_ACK or SPI_NACK, seq, len (u16), data[len], crc32 (u32)
// The response always fits in rx_len, so the next transfer reads all of it.
// If a read gets SPI_LISTENING instead, we never got the request, and the host sends it again.
// Anything that isn't a valid request is dropped, and we go back to listening.
// NACK means a valid request that we refused, e.g. CAN TX buffers are full.
//
// We arm what's next on every CS rising edge, and only trust a transfer if it was armed
// cleanly (see llspi_arm), otherwise we arm the same thing again. It never matters how
// long we take to get to it.
#define SPI_LISTENING 0x3CU
#define SPI_ACK 0xC3U
#define SPI_NACK 0x5AU
#define SPI_IDLE 0x00U  // sent on underrun

#define SPI_REQ_HEADER_SIZE 6U
#define SPI_RESP_HEADER_SIZE 4U
#define SPI_CRC_SIZE 4U
#define SPI_RESP_OVERHEAD (SPI_RESP_HEADER_SIZE + SPI_CRC_SIZE)

#define SPI_PROTOCOL_VERSION 3U
#define SPI_ENDPOINT_INFO 0xF0U

uint16_t spi_error_count = 0;

// low level SPI prototypes
void llspi_init(void);
bool llspi_arm(const uint8_t *tx, uint16_t tx_len);
uint16_t llspi_rx_count(void);

static bool spi_can_tx_ready = false;

static uint32_t read_u32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8U) | ((uint32_t)p[2] << 16U) | ((uint32_t)p[3] << 24U);
}

static void write_u32(uint8_t *p, uint32_t v) {
  p[0] = v & 0xFFU;
  p[1] = (v >> 8U) & 0xFFU;
  p[2] = (v >> 16U) & 0xFFU;
  p[3] = (v >> 24U) & 0xFFU;
}

// Handle one endpoint. Returns whether to ACK, and writes the response data length
static bool spi_handle_endpoint(uint8_t endpoint, const uint8_t *data, uint16_t data_len, uint16_t max_resp_len,
                                uint8_t *resp, uint16_t *resp_len) {
  bool ack = false;
  *resp_len = 0U;

  if (endpoint == 0U) {
    if (data_len >= sizeof(ControlPacket_t)) {
      ControlPacket_t ctrl = {0};
      (void)memcpy((uint8_t*)&ctrl, data, sizeof(ControlPacket_t));
      *resp_len = (uint16_t)comms_control_handler(&ctrl, resp);
      ack = true;
    }
  } else if ((endpoint == 1U) || (endpoint == 0x81U)) {
    *resp_len = (uint16_t)comms_can_read(resp, max_resp_len);
    ack = true;
  } else if (endpoint == 2U) {
    comms_endpoint2_write(data, data_len);
    ack = true;
  } else if (endpoint == 3U) {
    // NACK while the CAN TX buffers are full, the host retries
    if (spi_can_tx_ready) {
      spi_can_tx_ready = false;
      comms_can_write(data, data_len);
      ack = true;
    }
  } else if (endpoint == SPI_ENDPOINT_INFO) {
    // serial, HW type, bootstub, protocol version
    (void)memcpy(resp, ((uint8_t *)UID_BASE), 12);
    resp[12] = hw_type;
    resp[13] = USB_PID & 0xFFU;
    resp[14] = SPI_PROTOCOL_VERSION;
    *resp_len = 15U;
    ack = true;
  } else if (endpoint == 0xABU) {
    // test endpoint: mimics panda -> device transfer
    *resp_len = max_resp_len;
    ack = true;
  } else {
    // 0xAC is a test endpoint that always NACKs
  }

  return ack;
}

// Handle a request and build the response in spi_buf_tx. Returns the response size the host
// will read, or 0 if it's not a valid request
static uint16_t spi_process_request(uint16_t n) {
  // n can include filler from the host's reads if we were slow, it's ignored
  uint16_t tx_len = spi_buf_rx[2] | (spi_buf_rx[3] << 8U);
  uint16_t rx_len = spi_buf_rx[4] | (spi_buf_rx[5] << 8U);
  uint32_t req_len = SPI_REQ_HEADER_SIZE + (uint32_t)tx_len + SPI_CRC_SIZE;
  bool valid = (n >= (SPI_REQ_HEADER_SIZE + SPI_CRC_SIZE)) && (req_len <= n) &&
               (crc32(spi_buf_rx, req_len - SPI_CRC_SIZE) == read_u32(&spi_buf_rx[req_len - SPI_CRC_SIZE])) &&
               (rx_len >= SPI_RESP_OVERHEAD) && (rx_len <= SPI_BUF_SIZE);

  if (valid) {
    uint16_t max_data_len = rx_len - SPI_RESP_OVERHEAD;
    uint16_t data_len = 0U;
    bool ack = spi_handle_endpoint(spi_buf_rx[0], &spi_buf_rx[SPI_REQ_HEADER_SIZE], tx_len, max_data_len,
                                   &spi_buf_tx[SPI_RESP_HEADER_SIZE], &data_len);
    // like USB, a response is cut off at what the host reads
    data_len = ack ? MIN(data_len, max_data_len) : 0U;

    spi_buf_tx[0] = ack ? SPI_ACK : SPI_NACK;
    spi_buf_tx[1] = spi_buf_rx[1];
    spi_buf_tx[2] = data_len & 0xFFU;
    spi_buf_tx[3] = (data_len >> 8U) & 0xFFU;
    uint16_t len = SPI_RESP_HEADER_SIZE + data_len;
    write_u32(&spi_buf_tx[len], crc32(spi_buf_tx, len));
  } else if (req_len <= n) {
    // looked like a request, but the CRC is wrong. the host's filler doesn't count, its length doesn't fit
    spi_error_count += 1U;
  } else {
    // host's filler, or a partial request
  }
  return valid ? rx_len : 0U;
}

// Called on every CS rising edge
void spi_cs_rising(void) {
  static bool responding = false;  // otherwise listening
  static bool clean = false;       // whether the transfer that just ended was armed cleanly
  static uint16_t resp_len = 0U;

  uint16_t n = llspi_rx_count();
  if (clean && (n > 0U)) {
    // the host read the response, or sent a request
    resp_len = responding ? 0U : spi_process_request(n);
    responding = (resp_len > 0U);
  }

  if (responding) {
    clean = llspi_arm(spi_buf_tx, resp_len);
  } else {
    spi_buf_tx[0] = SPI_LISTENING;
    clean = llspi_arm(spi_buf_tx, 1U);
  }
}

void spi_init(void) {
  crc32_init();
  llspi_init();
  spi_cs_rising();
}

void can_tx_comms_resume_spi(void) {
  spi_can_tx_ready = true;
}
