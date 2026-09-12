#pragma once

#ifdef BOOTSTUB
#include "board/drivers/spi_legacy.h"
#else
#include "board/drivers/drivers.h"
#include "board/spi_protocol.h"
#if defined(STM32H7) && !defined(PANDA_JUNGLE) && !defined(PANDA_BODY)
#include "board/stm32h7/llspi_crc.h"
#endif

// One RX and two TX buffers in D2 SRAM let DMA replay cached replies directly.
// Only the request cache is CPU-owned; the cached TX buffer remains immutable.
__attribute__((section(".sram12"))) uint8_t spi_buf_rx[SPI_BUF_SIZE];
__attribute__((section(".sram12"), aligned(4))) uint8_t spi_buf_tx[SPI_BUF_SIZE];
static uint8_t *spi_tx_buffer = spi_buf_tx;
static uint8_t *spi_cached_response;
static uint16_t spi_cached_request_len;
static uint16_t spi_cached_response_len;
static bool spi_bound;
static bool spi_transmitting;
uint16_t spi_error_count = 0U;
#if defined(STM32H7) && !defined(PANDA_JUNGLE) && !defined(PANDA_BODY)
static bool spi_crc_hardware_active;
#endif


static uint32_t spi_crc(const uint8_t *data, uint16_t len) {
  uint32_t crc;
#if defined(STM32H7) && !defined(PANDA_JUNGLE) && !defined(PANDA_BODY)
  if (spi_crc_hardware_active) {
    crc = llspi_crc32_hw(data, len);
  } else
#endif
  {
    crc = spi_proto_crc32(data, len);
  }
  return crc;
}

void llspi_init(void);
void llspi_mosi_dma(uint8_t *addr, int len);
void llspi_miso_dma(const uint8_t *addr, int len);

static void spi_arm_rx(void) {
  spi_transmitting = false;
  llspi_mosi_dma(spi_buf_rx, SPI_BUF_SIZE);
}

static void spi_arm_tx(uint16_t len) {
  spi_transmitting = true;
  llspi_miso_dma(spi_tx_buffer, len);
}

static uint16_t spi_make_response(const spi_proto_header *req, uint8_t status, uint16_t len) {
  spi_proto_header reply = {
    .magic = SPI_PROTO_RESPONSE, .endpoint = status, .len = len,
    .seq = req->seq, .session = req->session, .capacity = 0U,
    .version = SPI_PROTO_VERSION, .reserved = 0U,
  };
  (void)memcpy(spi_tx_buffer, (const uint8_t *)&reply, sizeof(reply));
  // Actual data is followed by zero padding, then the CRC at the capacity boundary.
  uint16_t crc_offset = SPI_PROTO_HEADER_SIZE + req->capacity;
  (void)memset(&spi_tx_buffer[SPI_PROTO_HEADER_SIZE + len], 0, req->capacity - len);
  uint32_t crc = spi_crc(spi_tx_buffer, crc_offset);
  (void)memcpy(&spi_tx_buffer[crc_offset], (const uint8_t *)&crc, sizeof(crc));
  return crc_offset + sizeof(crc);
}

static uint16_t spi_version_response(void) {
  (void)memcpy(spi_tx_buffer, "VERSION", 7U);
  spi_tx_buffer[7] = 15U;
  spi_tx_buffer[8] = 0U;
  (void)memcpy(&spi_tx_buffer[9], (const uint8_t *)UID_BASE, 12U);
  spi_tx_buffer[21] = hw_type;
  spi_tx_buffer[22] = USB_PID & 0xFFU;
  spi_tx_buffer[23] = SPI_PROTO_VERSION;
  spi_tx_buffer[24] = crc_checksum(spi_tx_buffer, 24U, 0xD5U);
  return 25U;
}

static void spi_handle_request(spi_proto_header req, uint16_t rx_len) {
  static uint8_t spi_cached_request[SPI_BUF_SIZE];
  static uint64_t spi_session;
  static uint32_t spi_sequence;
  uint8_t status = SPI_PROTO_OK;
  uint16_t response_len = 0U;
  bool cache = false;
  bool replay = false;
  if (req.endpoint == SPI_PROTO_INIT) {
    if ((req.seq != 0U) || (req.session == 0U) || (req.len != 0U) || (req.capacity != 0U)) {
      status = SPI_PROTO_REJECTED;
#if defined(STM32H7) && !defined(PANDA_JUNGLE) && !defined(PANDA_BODY)
    } else if (!spi_crc_hardware_active) {
      // Software fallback can frame this small failure reply, but cannot meet
      // the normal v3 timing contract. Do not admit operations in that mode.
      spi_bound = false;
      status = SPI_PROTO_SESSION;
#endif
    } else if (!spi_bound || (req.session != spi_session)) {
      spi_bound = true;
      spi_session = req.session;
      spi_sequence = 0U;
      spi_cached_request_len = 0U;
      spi_cached_response_len = 0U;
      spi_cached_response = NULL;
      comms_can_reset();
    } else {
      // Repeated INIT preserves progress and the cached reply.
    }
  } else if (!spi_bound || (req.session != spi_session)) {
    status = SPI_PROTO_SESSION;
  } else if ((req.seq == spi_sequence) && (spi_cached_request_len != 0U)) {
    if ((rx_len == spi_cached_request_len) && (memcmp(spi_buf_rx, spi_cached_request, rx_len) == 0)) {
      spi_tx_buffer = spi_cached_response;
      replay = true;
    } else {
      status = SPI_PROTO_SEQUENCE;
    }
  } else if ((req.seq == 0U) || (spi_sequence == UINT32_MAX) || (req.seq != (spi_sequence + 1U))) {
    status = SPI_PROTO_SEQUENCE;
  } else {
    const uint8_t *payload = &spi_buf_rx[SPI_PROTO_HEADER_SIZE];
    uint8_t *response_data = &spi_tx_buffer[SPI_PROTO_HEADER_SIZE];
    cache = true;
    if ((req.endpoint == 0U) && (req.len == sizeof(ControlPacket_t))) {
      ControlPacket_t ctrl = {0};
      (void)memcpy((uint8_t *)&ctrl, payload, sizeof(ctrl));
      ctrl.length = MIN(ctrl.length, req.capacity);
      int actual = comms_control_handler(&ctrl, response_data);
      response_len = (uint16_t)MIN(MAX(actual, 0), req.capacity);
    } else if (((req.endpoint == 1U) || (req.endpoint == 0x81U)) && (req.len == 0U)) {
      response_len = (uint16_t)comms_can_read_spi(response_data, req.capacity);
    } else if ((req.endpoint == 2U) && (req.len > 0U)) {
      comms_endpoint2_write(payload, req.len);
    } else if ((req.endpoint == 3U) && (req.len > 0U)) {
      if (!comms_can_write_checked(payload, req.len)) {
        status = SPI_PROTO_REJECTED;
      }
      if ((status == SPI_PROTO_OK) && (req.capacity > 0U)) {
        // Cache admission and the dequeued RX bytes as one terminal operation.
        // A transport retry replays both effects without executing either again.
        response_len = (uint16_t)comms_can_read_spi(response_data, req.capacity);
      }
    } else if (req.endpoint == 0xABU) {
      response_len = req.capacity;
      (void)memset(response_data, 0, response_len);
    } else {
      status = SPI_PROTO_REJECTED;
    }
  }
  uint16_t frame_len = spi_cached_response_len;
  if (!replay) {
    frame_len = spi_make_response(&req, status, response_len);
  }
  if (cache) {
    (void)memcpy(spi_cached_request, spi_buf_rx, rx_len);
    spi_cached_response = spi_tx_buffer;
    spi_cached_request_len = rx_len;
    spi_cached_response_len = frame_len;
    spi_sequence = req.seq;
  }
  spi_arm_tx(frame_len);
}

// CS rising ends every TX frame, even a short read. Invalid requests have no effects.
void spi_cs_end(uint16_t rx_len, bool valid) {
  static const uint8_t version[] = {'V', 'E', 'R', 'S', 'I', 'O', 'N'};
  if (spi_transmitting || (valid && (rx_len == 0U))) {
    // Host runtime PM can toggle NSS without a request. Zero bytes can also
    // mean fewer than eight clocks; reset/rearm discards either case.
    spi_arm_rx();
  } else {
    __attribute__((section(".sram12"), aligned(4))) static uint8_t spi_buf_tx_other[SPI_BUF_SIZE];
    // Building errors or a new reply must never overwrite the cached DMA data.
    spi_tx_buffer = (spi_cached_response == spi_buf_tx) ? spi_buf_tx_other : spi_buf_tx;
    if (valid && (rx_len == sizeof(version)) && (memcmp(spi_buf_rx, version, sizeof(version)) == 0)) {
      spi_arm_tx(spi_version_response());
    } else if (!valid || (rx_len < SPI_PROTO_OVERHEAD)) {
      spi_error_count++;
      spi_arm_rx();
    } else {
      spi_proto_header req = {0};
      uint32_t received_crc = 0U;
      (void)memcpy((uint8_t *)&req, spi_buf_rx, sizeof(req));
      (void)memcpy((uint8_t *)&received_crc, &spi_buf_rx[rx_len - sizeof(received_crc)], sizeof(received_crc));
      bool header_valid = (req.magic == SPI_PROTO_REQUEST) && (req.version == SPI_PROTO_VERSION) && (req.reserved == 0U);
      bool length_valid = (req.len <= SPI_PROTO_MAX_PAYLOAD) && (req.capacity <= SPI_PROTO_MAX_PAYLOAD) &&
                          (rx_len == (SPI_PROTO_OVERHEAD + req.len));
      if (header_valid && length_valid && (received_crc == spi_crc(spi_buf_rx, rx_len - sizeof(received_crc)))) {
        spi_handle_request(req, rx_len);
      } else {
        spi_error_count++;
        spi_arm_rx();
      }
    }
  }
}

void spi_init(void) {
  spi_bound = false;
  spi_cached_request_len = 0U;
  spi_cached_response_len = 0U;
  spi_cached_response = NULL;
  spi_tx_buffer = spi_buf_tx;
#if defined(STM32H7) && !defined(PANDA_JUNGLE) && !defined(PANDA_BODY)
  spi_crc_hardware_active = llspi_crc_init();
#endif
  llspi_init();
  spi_arm_rx();
}

void can_tx_comms_resume_spi(void) {
  // v3 checks exact record counts atomically when admitting each CAN write.
}
#endif
