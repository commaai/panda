#pragma once

#define ISOTP_MSG_MAX_LEN 4095U
#define ISOTP_N_WFTMAX 10U
#define ISOTP_DEFAULT_RX_STMIN 0U
#define ISOTP_DEFAULT_TX_MESSAGE_TIMEOUT_MS 1000U
#define ISOTP_DEFAULT_TX_TRANSFER_TIMEOUT_MS 10000U

#define ISOTP_TX_RING_SIZE 16384U
#define ISOTP_RX_RING_SIZE 16384U
#define ISOTP_BULK_MAX_RECORD_SIZE (ISOTP_MSG_MAX_LEN + 2U)
#define ISOTP_BULK_WRITE_BUFFER_SIZE (ISOTP_BULK_MAX_RECORD_SIZE + SPI_BUF_SIZE)

enum isotp_tx_state {
  ISOTP_TX_IDLE = 0,
  ISOTP_TX_WAIT_FC,
  ISOTP_TX_WAIT_STMIN,
};

enum isotp_rx_state {
  ISOTP_RX_IDLE = 0,
  ISOTP_RX_WAIT_CF,
};

enum isotp_frame_type {
  ISOTP_FRAME_INVALID = 0,
  ISOTP_FRAME_SINGLE,
  ISOTP_FRAME_FIRST,
  ISOTP_FRAME_CONSECUTIVE,
  ISOTP_FRAME_FLOW_CONTROL,
};

typedef struct {
  volatile uint32_t w_ptr;
  volatile uint32_t r_ptr;
  uint32_t fifo_size;
  uint8_t *elems;
} isotp_byte_ring_t;

typedef struct {
  uint16_t ptr;
  uint8_t data[ISOTP_BULK_MAX_RECORD_SIZE];
} isotp_bulk_read_buffer_t;

typedef struct {
  uint32_t len;
  uint8_t data[ISOTP_BULK_WRITE_BUFFER_SIZE];
} isotp_bulk_write_buffer_t;

typedef struct {
  enum isotp_frame_type type;
  const uint8_t *data;
  uint8_t len;
  uint16_t total_len;
  uint8_t flow_status;
  uint8_t block_size;
  uint8_t stmin_raw;
  uint8_t sn;
} isotp_parsed_frame_t;

typedef struct {
  bool configured;
  bool bus_set;
  bool tx_id_set;
  bool rx_id_set;
  uint8_t bus;

  uint32_t tx_id;
  uint32_t rx_id;
  bool tx_id_extended;
  bool rx_id_extended;

  bool tx_ext_addr_enabled;
  uint8_t tx_ext_addr;
  bool rx_ext_addr_enabled;
  uint8_t rx_ext_addr;
  uint16_t tx_message_timeout_ms;
  uint16_t tx_transfer_timeout_ms;

  struct {
    uint8_t buf[ISOTP_MSG_MAX_LEN];
    uint16_t len;
    uint16_t offset;
    uint8_t next_sn;
    uint8_t block_size;
    uint8_t block_cf_sent;
    uint8_t wait_fc_count;
    uint32_t stmin_us;
    uint32_t deadline_us;
    uint32_t transfer_deadline_us;
    uint32_t next_cf_us;
    enum isotp_tx_state state;
  } tx;

  struct {
    uint8_t buf[ISOTP_MSG_MAX_LEN];
    uint16_t expected_len;
    uint16_t offset;
    uint8_t next_sn;
    enum isotp_rx_state state;
  } rx;
} isotp_session_t;

#ifdef STM32H7
__attribute__((section(".axisram"))) static uint8_t elems_isotp_tx_q[ISOTP_TX_RING_SIZE];
static isotp_byte_ring_t isotp_tx_q = {
  .w_ptr = 0U,
  .r_ptr = 0U,
  .fifo_size = ISOTP_TX_RING_SIZE,
  .elems = elems_isotp_tx_q,
};
__attribute__((section(".axisram"))) static uint8_t elems_isotp_rx_q[ISOTP_RX_RING_SIZE];
static isotp_byte_ring_t isotp_rx_q = {
  .w_ptr = 0U,
  .r_ptr = 0U,
  .fifo_size = ISOTP_RX_RING_SIZE,
  .elems = elems_isotp_rx_q,
};
__attribute__((section(".sram12"))) static isotp_bulk_read_buffer_t isotp_read_buffer = {0};
__attribute__((section(".sram12"))) static isotp_bulk_write_buffer_t isotp_write_buffer = {0};
#else
static uint8_t elems_isotp_tx_q[ISOTP_TX_RING_SIZE];
static isotp_byte_ring_t isotp_tx_q = {
  .w_ptr = 0U,
  .r_ptr = 0U,
  .fifo_size = ISOTP_TX_RING_SIZE,
  .elems = elems_isotp_tx_q,
};
static uint8_t elems_isotp_rx_q[ISOTP_RX_RING_SIZE];
static isotp_byte_ring_t isotp_rx_q = {
  .w_ptr = 0U,
  .r_ptr = 0U,
  .fifo_size = ISOTP_RX_RING_SIZE,
  .elems = elems_isotp_rx_q,
};
static isotp_bulk_read_buffer_t isotp_read_buffer = {0};
static isotp_bulk_write_buffer_t isotp_write_buffer = {0};
#endif

static isotp_session_t isotp_session = {
  .configured = false,
  .bus_set = false,
  .tx_id_set = false,
  .rx_id_set = false,
  .tx_message_timeout_ms = ISOTP_DEFAULT_TX_MESSAGE_TIMEOUT_MS,
  .tx_transfer_timeout_ms = ISOTP_DEFAULT_TX_TRANSFER_TIMEOUT_MS,
  .tx = {
    .state = ISOTP_TX_IDLE,
  },
  .rx = {
    .state = ISOTP_RX_IDLE,
  },
};

static void isotp_abort_tx(void);
static void isotp_abort_rx(void);
static void isotp_kick(uint32_t now_us);
int comms_isotp_read(uint8_t *data, uint32_t max_len);
void comms_isotp_write(const uint8_t *data, uint32_t len);
void comms_isotp_reset(void);
bool comms_isotp_can_write_usb(void);
bool comms_isotp_can_write_spi(uint32_t len);

static bool isotp_bulk_len_valid(uint16_t len) {
  return (len > 0U) && (len <= ISOTP_MSG_MAX_LEN);
}

// Records are stored as [len_lo, len_hi, payload...]. Producers either queue
// a whole record or nothing, and consumers pop one whole record at a time.
// Any malformed/incomplete record is treated as queue corruption.
static void isotp_byte_ring_clear(isotp_byte_ring_t *q) {
  ENTER_CRITICAL();
  q->w_ptr = 0U;
  q->r_ptr = 0U;
  EXIT_CRITICAL();
}

static bool isotp_byte_ring_push(isotp_byte_ring_t *q, const uint8_t *data, uint32_t len) {
  bool ret = false;

  ENTER_CRITICAL();
  uint32_t free_bytes;
  if (q->w_ptr >= q->r_ptr) {
    free_bytes = q->fifo_size - 1U - q->w_ptr + q->r_ptr;
  } else {
    free_bytes = q->r_ptr - q->w_ptr - 1U;
  }

  if (free_bytes >= len) {
    for (uint32_t i = 0U; i < len; i++) {
      q->elems[q->w_ptr] = data[i];
      q->w_ptr = (q->w_ptr + 1U) % q->fifo_size;
    }
    ret = true;
  }
  EXIT_CRITICAL();

  return ret;
}

// Pop one complete length-prefixed record under a single critical section so
// validation and dequeueing are atomic with respect to producers.
static bool isotp_byte_ring_pop_record(isotp_byte_ring_t *q, uint8_t *data, uint16_t *record_len) {
  bool ret = false;

  ENTER_CRITICAL();
  uint32_t used_bytes;
  if (q->w_ptr >= q->r_ptr) {
    used_bytes = q->w_ptr - q->r_ptr;
  } else {
    used_bytes = q->fifo_size - q->r_ptr + q->w_ptr;
  }

  if (used_bytes >= 2U) {
    uint32_t ptr = q->r_ptr;
    uint8_t hdr[2] = {q->elems[ptr], 0U};
    ptr = (ptr + 1U) % q->fifo_size;
    hdr[1] = q->elems[ptr];

    uint16_t payload_len = hdr[0] | ((uint16_t)hdr[1] << 8);
    uint16_t full_len = payload_len + 2U;
    if (!isotp_bulk_len_valid(payload_len)) {
      q->w_ptr = 0U;
      q->r_ptr = 0U;
    } else if (used_bytes >= full_len) {
      data[0] = hdr[0];
      data[1] = hdr[1];
      for (uint16_t i = 0U; i < payload_len; i++) {
        ptr = (ptr + 1U) % q->fifo_size;
        data[i + 2U] = q->elems[ptr];
      }
      q->r_ptr = (q->r_ptr + full_len) % q->fifo_size;
      *record_len = full_len;
      ret = true;
    } else {
      // Partial records are not expected from queue producers.
    }
  }
  EXIT_CRITICAL();

  return ret;
}

static bool isotp_byte_ring_pop_record_payload(isotp_byte_ring_t *q, uint8_t *data, uint16_t *payload_len) {
  bool ret = false;

  ENTER_CRITICAL();
  uint32_t used_bytes;
  if (q->w_ptr >= q->r_ptr) {
    used_bytes = q->w_ptr - q->r_ptr;
  } else {
    used_bytes = q->fifo_size - q->r_ptr + q->w_ptr;
  }

  if (used_bytes >= 2U) {
    uint32_t ptr = q->r_ptr;
    uint8_t hdr[2] = {q->elems[ptr], 0U};
    ptr = (ptr + 1U) % q->fifo_size;
    hdr[1] = q->elems[ptr];

    uint16_t len = hdr[0] | ((uint16_t)hdr[1] << 8);
    uint16_t full_len = len + 2U;
    if (!isotp_bulk_len_valid(len)) {
      q->w_ptr = 0U;
      q->r_ptr = 0U;
    } else if (used_bytes >= full_len) {
      for (uint16_t i = 0U; i < len; i++) {
        ptr = (ptr + 1U) % q->fifo_size;
        data[i] = q->elems[ptr];
      }
      q->r_ptr = (q->r_ptr + full_len) % q->fifo_size;
      *payload_len = len;
      ret = true;
    } else {
      // Partial records are not expected from queue producers.
    }
  }
  EXIT_CRITICAL();

  return ret;
}

// USB/SPI bulk transfers can split records arbitrarily, so the queue keeps the
// length prefix with the payload instead of relying on packet boundaries.
static bool isotp_byte_ring_push_record(isotp_byte_ring_t *q, const uint8_t *payload, uint16_t len) {
  bool ret = false;
  uint32_t record_len = ((uint32_t)len) + 2U;

  if (isotp_bulk_len_valid(len)) {
    ENTER_CRITICAL();
    uint32_t free_bytes;
    if (q->w_ptr >= q->r_ptr) {
      free_bytes = q->fifo_size - 1U - q->w_ptr + q->r_ptr;
    } else {
      free_bytes = q->r_ptr - q->w_ptr - 1U;
    }

    if (free_bytes >= record_len) {
      q->elems[q->w_ptr] = len & 0xFFU;
      q->w_ptr = (q->w_ptr + 1U) % q->fifo_size;
      q->elems[q->w_ptr] = (len >> 8) & 0xFFU;
      q->w_ptr = (q->w_ptr + 1U) % q->fifo_size;
      for (uint16_t i = 0U; i < len; i++) {
        q->elems[q->w_ptr] = payload[i];
        q->w_ptr = (q->w_ptr + 1U) % q->fifo_size;
      }
      ret = true;
    }
    EXIT_CRITICAL();
  }

  return ret;
}

static void refresh_isotp_tx_slots_available(void) {
  if (comms_isotp_can_write_usb()) {
    isotp_tx_comms_resume_usb();
  }
  if (comms_isotp_can_write_spi(1U)) {
    isotp_tx_comms_resume_spi();
  }
}

static void isotp_shift_write_buffer_left(uint32_t shift) {
  if ((shift != 0U) && (shift <= isotp_write_buffer.len)) {
    uint32_t remaining = isotp_write_buffer.len - shift;
    for (uint32_t i = 0U; i < remaining; i++) {
      isotp_write_buffer.data[i] = isotp_write_buffer.data[i + shift];
    }
    isotp_write_buffer.len = remaining;
  }
}

static void isotp_drain_bulk_write_buffer(void) {
  bool done = false;

  while ((isotp_write_buffer.len >= 2U) && !done) {
    uint16_t payload_len = isotp_write_buffer.data[0] | ((uint16_t)isotp_write_buffer.data[1] << 8);
    uint32_t record_len = (uint32_t)payload_len + 2U;

    if (!isotp_bulk_len_valid(payload_len)) {
      isotp_write_buffer.len = 0U;
      done = true;
    } else if (isotp_write_buffer.len < record_len) {
      done = true;
    } else if (!isotp_byte_ring_push(&isotp_tx_q, isotp_write_buffer.data, record_len)) {
      done = true;
    } else {
      isotp_shift_write_buffer_left(record_len);
    }
  }

  refresh_isotp_tx_slots_available();
}

int comms_isotp_read(uint8_t *data, uint32_t max_len) {
  uint32_t pos = 0U;

  while (pos < max_len) {
    if (isotp_read_buffer.ptr == 0U) {
      if (!isotp_byte_ring_pop_record(&isotp_rx_q, isotp_read_buffer.data, &isotp_read_buffer.ptr)) {
        break;
      }
    }

    uint32_t copy_len = MIN(max_len - pos, (uint32_t)isotp_read_buffer.ptr);
    (void)memcpy(&data[pos], isotp_read_buffer.data, copy_len);
    pos += copy_len;

    uint32_t remaining = (uint32_t)isotp_read_buffer.ptr - copy_len;
    for (uint32_t i = 0U; i < remaining; i++) {
      isotp_read_buffer.data[i] = isotp_read_buffer.data[i + copy_len];
    }
    isotp_read_buffer.ptr = remaining;
  }

  return pos;
}

void comms_isotp_write(const uint8_t *data, uint32_t len) {
  uint32_t copy_len = MIN(len, (uint32_t)(sizeof(isotp_write_buffer.data) - isotp_write_buffer.len));

  if (copy_len > 0U) {
    (void)memcpy(&isotp_write_buffer.data[isotp_write_buffer.len], data, copy_len);
    isotp_write_buffer.len += copy_len;
    isotp_drain_bulk_write_buffer();
  }
}

bool comms_isotp_can_write_usb(void) {
  return ((sizeof(isotp_write_buffer.data) - isotp_write_buffer.len) >= USBPACKET_MAX_SIZE);
}

bool comms_isotp_can_write_spi(uint32_t len) {
  return ((sizeof(isotp_write_buffer.data) - isotp_write_buffer.len) >= len);
}

static void isotp_reset_tx_state(void) {
  isotp_session.tx.len = 0U;
  isotp_session.tx.offset = 0U;
  isotp_session.tx.next_sn = 0U;
  isotp_session.tx.block_size = 0U;
  isotp_session.tx.block_cf_sent = 0U;
  isotp_session.tx.wait_fc_count = 0U;
  isotp_session.tx.stmin_us = 0U;
  isotp_session.tx.deadline_us = 0U;
  isotp_session.tx.transfer_deadline_us = 0U;
  isotp_session.tx.next_cf_us = 0U;
  isotp_session.tx.state = ISOTP_TX_IDLE;
}

static void isotp_reset_rx_state(void) {
  isotp_session.rx.expected_len = 0U;
  isotp_session.rx.offset = 0U;
  isotp_session.rx.next_sn = 0U;
  isotp_session.rx.state = ISOTP_RX_IDLE;
}

static void isotp_abort_tx(void) {
  isotp_reset_tx_state();
}

static void isotp_abort_rx(void) {
  isotp_reset_rx_state();
}

void comms_isotp_reset(void) {
  isotp_write_buffer.len = 0U;
  isotp_read_buffer.ptr = 0U;
  isotp_byte_ring_clear(&isotp_tx_q);
  isotp_byte_ring_clear(&isotp_rx_q);
  isotp_abort_tx();
  isotp_abort_rx();
  refresh_isotp_tx_slots_available();
}

static void __attribute__((unused)) isotp_update_configured(void) {
  isotp_session.configured = isotp_session.bus_set && isotp_session.tx_id_set && isotp_session.rx_id_set;
}

static uint32_t isotp_deadline_from_ms(uint32_t now_us, uint16_t timeout_ms) {
  return now_us + ((uint32_t)timeout_ms * 1000U);
}

static bool isotp_time_reached(uint32_t now_us, uint32_t target_us) {
  return (get_ts_elapsed(now_us, target_us) < 0x80000000U);
}

static uint8_t isotp_single_frame_capacity(bool ext_addr_enabled) {
  return ext_addr_enabled ? 6U : 7U;
}

static uint8_t isotp_first_frame_capacity(bool ext_addr_enabled) {
  return ext_addr_enabled ? 5U : 6U;
}

static uint8_t isotp_consecutive_frame_capacity(bool ext_addr_enabled) {
  return ext_addr_enabled ? 6U : 7U;
}

static bool __attribute__((unused)) isotp_parse_packed_arb_id(uint32_t packed_id, uint32_t *arb_id, bool *extended) {
  bool ret = ((packed_id & 0x60000000U) == 0U);
  bool ext = (packed_id & 0x80000000U) != 0U;
  uint32_t raw_id = packed_id & 0x1FFFFFFFU;

  if ((!ext) && ((raw_id & ~0x7FFU) != 0U)) {
    ret = false;
  }

  if (ret) {
    *arb_id = raw_id;
    *extended = ext;
  }

  return ret;
}

static void __attribute__((unused)) isotp_set_bus(uint8_t bus) {
  isotp_session.bus = bus;
  isotp_session.bus_set = true;
  isotp_update_configured();
  comms_isotp_reset();
}

static bool __attribute__((unused)) isotp_set_tx_arb_id(uint32_t packed_id) {
  uint32_t arb_id;
  bool extended;
  bool ret = isotp_parse_packed_arb_id(packed_id, &arb_id, &extended);

  if (ret) {
    isotp_session.tx_id = arb_id;
    isotp_session.tx_id_extended = extended;
    isotp_session.tx_id_set = true;
    isotp_update_configured();
    comms_isotp_reset();
  }

  return ret;
}

static bool __attribute__((unused)) isotp_set_rx_arb_id(uint32_t packed_id) {
  uint32_t arb_id;
  bool extended;
  bool ret = isotp_parse_packed_arb_id(packed_id, &arb_id, &extended);

  if (ret) {
    isotp_session.rx_id = arb_id;
    isotp_session.rx_id_extended = extended;
    isotp_session.rx_id_set = true;
    isotp_update_configured();
    comms_isotp_reset();
  }

  return ret;
}

static void __attribute__((unused)) isotp_set_ext_addr(uint16_t tx_cfg, uint16_t rx_cfg) {
  isotp_session.tx_ext_addr = tx_cfg & 0xFFU;
  isotp_session.tx_ext_addr_enabled = ((tx_cfg >> 8U) & 0x1U) != 0U;
  isotp_session.rx_ext_addr = rx_cfg & 0xFFU;
  isotp_session.rx_ext_addr_enabled = ((rx_cfg >> 8U) & 0x1U) != 0U;
  comms_isotp_reset();
}

static void __attribute__((unused)) isotp_set_tx_timeouts(uint16_t message_timeout_ms, uint16_t transfer_timeout_ms) {
  isotp_session.tx_message_timeout_ms = message_timeout_ms;
  isotp_session.tx_transfer_timeout_ms = transfer_timeout_ms;
}

static bool isotp_send_can_frame(const uint8_t *payload, uint8_t payload_len) {
  CANPacket_t pkt = {0};
  uint8_t frame_len = payload_len;
  uint8_t data_offset = 0U;

  if (isotp_session.tx_ext_addr_enabled) {
    pkt.data[0] = isotp_session.tx_ext_addr;
    data_offset = 1U;
    frame_len += 1U;
  }

  pkt.fd = 0U;
  pkt.returned = 0U;
  pkt.rejected = 0U;
  pkt.extended = isotp_session.tx_id_extended;
  pkt.addr = isotp_session.tx_id;
  pkt.bus = isotp_session.bus;
  pkt.data_len_code = frame_len;
  (void)memcpy(&pkt.data[data_offset], payload, payload_len);
  can_set_checksum(&pkt);
  can_send(&pkt, isotp_session.bus, false);

  return (pkt.rejected == 0U);
}

static uint32_t isotp_decode_stmin_us(uint8_t stmin_raw) {
  uint32_t ret = 0U;

  if (stmin_raw <= 0x7FU) {
    ret = ((uint32_t)stmin_raw * 1000U);
  } else if ((stmin_raw >= 0xF1U) && (stmin_raw <= 0xF9U)) {
    ret = 1000U;
  } else {
    ret = 127000U;
  }

  return ret;
}

static bool isotp_match_rx_frame(const CANPacket_t *msg) {
  bool msg_extended = msg->extended != 0U;

  return (msg->fd == 0U) &&
         (msg->returned == 0U) &&
         (msg->rejected == 0U) &&
         (msg->bus == isotp_session.bus) &&
         (msg_extended == isotp_session.rx_id_extended) &&
         (msg->addr == isotp_session.rx_id);
}

static isotp_parsed_frame_t isotp_parse_frame(const CANPacket_t *msg) {
  isotp_parsed_frame_t parsed = {
    .type = ISOTP_FRAME_INVALID,
    .data = NULL,
    .len = 0U,
    .total_len = 0U,
    .flow_status = 0U,
    .block_size = 0U,
    .stmin_raw = 0U,
    .sn = 0U,
  };

  uint8_t actual_len = dlc_to_len[msg->data_len_code];
  uint8_t data_offset = 0U;
  bool parse_ok = true;

  if (isotp_session.rx_ext_addr_enabled) {
    if ((actual_len == 0U) || (msg->data[0] != isotp_session.rx_ext_addr)) {
      parse_ok = false;
    } else {
      data_offset = 1U;
    }
  }

  if (actual_len <= data_offset) {
    parse_ok = false;
  }

  if (parse_ok) {
    const uint8_t *data = &msg->data[data_offset];
    uint8_t frame_len = actual_len - data_offset;

    switch (data[0] & 0xF0U) {
      case 0x00U:
        parsed.len = data[0] & 0x0FU;
        if ((parsed.len == 0U) || ((uint8_t)(parsed.len + 1U) > frame_len)) {
          break;
        }
        parsed.type = ISOTP_FRAME_SINGLE;
        parsed.data = &data[1];
        break;
      case 0x10U:
        if (frame_len < 2U) {
          break;
        }
        parsed.total_len = ((uint16_t)(data[0] & 0x0FU) << 8U) | data[1];
        if ((parsed.total_len == 0U) || (parsed.total_len > ISOTP_MSG_MAX_LEN)) {
          break;
        }
        parsed.type = ISOTP_FRAME_FIRST;
        parsed.data = &data[2];
        parsed.len = frame_len - 2U;
        break;
      case 0x20U:
        if (frame_len < 2U) {
          break;
        }
        parsed.type = ISOTP_FRAME_CONSECUTIVE;
        parsed.sn = data[0] & 0x0FU;
        parsed.data = &data[1];
        parsed.len = frame_len - 1U;
        break;
      case 0x30U:
        if (frame_len < 3U) {
          break;
        }
        parsed.type = ISOTP_FRAME_FLOW_CONTROL;
        parsed.flow_status = data[0] & 0x0FU;
        parsed.block_size = data[1];
        parsed.stmin_raw = data[2];
        break;
      default:
        break;
    }
  }

  return parsed;
}

static bool isotp_send_flow_control(uint8_t flow_status, uint8_t block_size, uint8_t stmin) {
  uint8_t fc[3] = {
    (uint8_t)(0x30U | (flow_status & 0x0FU)),
    block_size,
    stmin,
  };
  return isotp_send_can_frame(fc, sizeof(fc));
}

static void isotp_handle_single_frame(const isotp_parsed_frame_t *parsed) {
  (void)isotp_byte_ring_push_record(&isotp_rx_q, parsed->data, parsed->len);
}

static void isotp_handle_first_frame(const isotp_parsed_frame_t *parsed) {
  uint8_t single_cap = isotp_single_frame_capacity(isotp_session.rx_ext_addr_enabled);

  if ((parsed->total_len > single_cap) && (parsed->len <= parsed->total_len)) {
    (void)memcpy(isotp_session.rx.buf, parsed->data, parsed->len);
    isotp_session.rx.expected_len = parsed->total_len;
    isotp_session.rx.offset = parsed->len;
    isotp_session.rx.next_sn = 1U;
    isotp_session.rx.state = ISOTP_RX_WAIT_CF;

    if (!isotp_send_flow_control(0U, 0U, ISOTP_DEFAULT_RX_STMIN)) {
      isotp_abort_rx();
    }
  }
}

static void isotp_finish_rx_if_complete(void) {
  if (isotp_session.rx.offset >= isotp_session.rx.expected_len) {
    (void)isotp_byte_ring_push_record(&isotp_rx_q, isotp_session.rx.buf, isotp_session.rx.expected_len);
    isotp_abort_rx();
  }
}

static void isotp_handle_consecutive_frame(const isotp_parsed_frame_t *parsed) {
  if (parsed->sn == isotp_session.rx.next_sn) {
    uint16_t remaining = isotp_session.rx.expected_len - isotp_session.rx.offset;
    uint8_t copy_len = MIN(remaining, parsed->len);
    (void)memcpy(&isotp_session.rx.buf[isotp_session.rx.offset], parsed->data, copy_len);
    isotp_session.rx.offset += copy_len;
    isotp_session.rx.next_sn = (isotp_session.rx.next_sn + 1U) & 0x0FU;
    isotp_finish_rx_if_complete();
  } else {
    isotp_abort_rx();
  }
}

static bool isotp_pop_next_tx_pdu(uint8_t *data, uint16_t *len) {
  return isotp_byte_ring_pop_record_payload(&isotp_tx_q, data, len);
}

static void isotp_try_start_next_tx(uint32_t now_us) {
  uint8_t frame[8];
  uint8_t single_cap = isotp_single_frame_capacity(isotp_session.tx_ext_addr_enabled);
  uint8_t ff_cap = isotp_first_frame_capacity(isotp_session.tx_ext_addr_enabled);
  bool start_tx = false;

  isotp_drain_bulk_write_buffer();

  if (isotp_session.configured &&
      (isotp_session.tx.state == ISOTP_TX_IDLE) &&
      (can_slots_empty(can_queues[isotp_session.bus]) > 0U)) {
    start_tx = isotp_pop_next_tx_pdu(isotp_session.tx.buf, &isotp_session.tx.len);
  }

  if (start_tx) {
    isotp_drain_bulk_write_buffer();
    isotp_session.tx.transfer_deadline_us = isotp_deadline_from_ms(now_us, isotp_session.tx_transfer_timeout_ms);

    if (isotp_session.tx.len <= single_cap) {
      bool sent_ok;
      frame[0] = isotp_session.tx.len & 0x0FU;
      (void)memcpy(&frame[1], isotp_session.tx.buf, isotp_session.tx.len);

      sent_ok = isotp_send_can_frame(frame, (uint8_t)(isotp_session.tx.len + 1U));
      if (!sent_ok) {
        // Safety rejected the SF. Drop the active PDU like any other TX failure.
      }
      isotp_abort_tx();
    } else {
      frame[0] = 0x10U | ((isotp_session.tx.len >> 8U) & 0x0FU);
      frame[1] = isotp_session.tx.len & 0xFFU;
      (void)memcpy(&frame[2], isotp_session.tx.buf, ff_cap);

      if (!isotp_send_can_frame(frame, (uint8_t)(ff_cap + 2U))) {
        isotp_abort_tx();
      } else {
        isotp_session.tx.offset = ff_cap;
        isotp_session.tx.next_sn = 1U;
        isotp_session.tx.block_cf_sent = 0U;
        isotp_session.tx.wait_fc_count = 0U;
        isotp_session.tx.stmin_us = 0U;
        isotp_session.tx.deadline_us = isotp_deadline_from_ms(now_us, isotp_session.tx_message_timeout_ms);
        isotp_session.tx.state = ISOTP_TX_WAIT_FC;
      }
    }
  }
}

static void isotp_try_send_consecutive_frames(uint32_t now_us) {
  uint8_t frame[8];
  uint8_t cf_cap = isotp_consecutive_frame_capacity(isotp_session.tx_ext_addr_enabled);
  bool tx_failed = false;

  while ((isotp_session.tx.offset < isotp_session.tx.len) &&
         !tx_failed &&
         (can_slots_empty(can_queues[isotp_session.bus]) > 0U) &&
         ((isotp_session.tx.block_size == 0U) || (isotp_session.tx.block_cf_sent < isotp_session.tx.block_size))) {
    uint16_t remaining = isotp_session.tx.len - isotp_session.tx.offset;
    uint8_t copy_len = MIN(remaining, cf_cap);

    frame[0] = 0x20U | (isotp_session.tx.next_sn & 0x0FU);
    (void)memcpy(&frame[1], &isotp_session.tx.buf[isotp_session.tx.offset], copy_len);

    if (!isotp_send_can_frame(frame, (uint8_t)(copy_len + 1U))) {
      isotp_abort_tx();
      tx_failed = true;
    } else {
      isotp_session.tx.offset += copy_len;
      isotp_session.tx.next_sn = (isotp_session.tx.next_sn + 1U) & 0x0FU;
      isotp_session.tx.block_cf_sent += 1U;

      if (isotp_session.tx.stmin_us != 0U) {
        break;
      }
    }
  }

  if (!tx_failed) {
    if (isotp_session.tx.offset >= isotp_session.tx.len) {
      isotp_abort_tx();
    } else if ((isotp_session.tx.block_size != 0U) && (isotp_session.tx.block_cf_sent >= isotp_session.tx.block_size)) {
      isotp_session.tx.deadline_us = isotp_deadline_from_ms(now_us, isotp_session.tx_message_timeout_ms);
      isotp_session.tx.state = ISOTP_TX_WAIT_FC;
    } else {
      isotp_session.tx.next_cf_us = (isotp_session.tx.stmin_us == 0U) ? now_us : (now_us + isotp_session.tx.stmin_us);
      isotp_session.tx.state = ISOTP_TX_WAIT_STMIN;
    }
  }
}

static void isotp_handle_flow_control(const isotp_parsed_frame_t *parsed, uint32_t now_us) {
  switch (parsed->flow_status) {
    case 0U:
      isotp_session.tx.block_size = parsed->block_size;
      isotp_session.tx.block_cf_sent = 0U;
      isotp_session.tx.stmin_us = isotp_decode_stmin_us(parsed->stmin_raw);
      isotp_session.tx.next_cf_us = now_us;
      isotp_session.tx.state = ISOTP_TX_WAIT_STMIN;
      break;
    case 1U:
      isotp_session.tx.wait_fc_count += 1U;
      if (isotp_session.tx.wait_fc_count > ISOTP_N_WFTMAX) {
        isotp_abort_tx();
      } else {
        isotp_session.tx.deadline_us = isotp_deadline_from_ms(now_us, isotp_session.tx_message_timeout_ms);
      }
      break;
    case 2U:
    default:
      isotp_abort_tx();
      break;
  }
}

static void isotp_kick(uint32_t now_us) {
  if (isotp_session.configured) {
    if (isotp_session.tx.state == ISOTP_TX_IDLE) {
      isotp_try_start_next_tx(now_us);
    }

    if ((isotp_session.tx.state == ISOTP_TX_WAIT_STMIN) &&
        isotp_time_reached(now_us, isotp_session.tx.next_cf_us)) {
      isotp_try_send_consecutive_frames(now_us);
    }
  }
}

void isotp_periodic_handler(uint32_t now_us) {
  if (isotp_session.configured) {
    if ((isotp_session.tx.state != ISOTP_TX_IDLE) &&
        isotp_time_reached(now_us, isotp_session.tx.transfer_deadline_us)) {
      isotp_abort_tx();
    }

    if ((isotp_session.tx.state == ISOTP_TX_WAIT_FC) &&
        isotp_time_reached(now_us, isotp_session.tx.deadline_us)) {
      isotp_abort_tx();
    }

    isotp_kick(now_us);
  }
}

void isotp_rx_hook(const CANPacket_t *msg, uint32_t now_us) {
  if (isotp_session.configured && isotp_match_rx_frame(msg)) {
    isotp_parsed_frame_t parsed = isotp_parse_frame(msg);

    if (parsed.type != ISOTP_FRAME_INVALID) {
      if ((parsed.type == ISOTP_FRAME_FLOW_CONTROL) &&
          (isotp_session.tx.state == ISOTP_TX_WAIT_FC)) {
        isotp_handle_flow_control(&parsed, now_us);
      } else {
        switch (isotp_session.rx.state) {
          case ISOTP_RX_IDLE:
            if (parsed.type == ISOTP_FRAME_SINGLE) {
              isotp_handle_single_frame(&parsed);
            } else if (parsed.type == ISOTP_FRAME_FIRST) {
              isotp_handle_first_frame(&parsed);
            } else {
              // Ignore unrelated frame types while idle.
            }
            break;
          case ISOTP_RX_WAIT_CF:
            if (parsed.type == ISOTP_FRAME_CONSECUTIVE) {
              isotp_handle_consecutive_frame(&parsed);
            } else if (parsed.type == ISOTP_FRAME_FIRST) {
              isotp_abort_rx();
              isotp_handle_first_frame(&parsed);
            } else {
              // Ignore unrelated frame types while waiting for CF.
            }
            break;
          default:
            break;
        }
      }

      isotp_kick(now_us);
    }
  }
}
