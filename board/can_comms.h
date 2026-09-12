#pragma once

/*
  CAN transactions to and from the host come in the form of
  a certain number of CANPacket_t. The transaction is split
  into multiple transfers or chunks.

  CAN packet byte layout (wire format used by comms_can_{read,write}):
  +--------+--------+--------+--------+--------+--------+--------+------------------------------+
  | byte 0 | byte 1 | byte 2 | byte 3 | byte 4 | byte 5 | byte 6 | ... byte 13 / byte 69        |
  +--------+--------+--------+--------+--------+--------+--------+------------------------------+
  | DLC    | addr   | addr   | addr   | flags  | cksum  | data0  | ... data7 / data63           |
  | bus    |        |        |        |        |        |        | (classic CAN / CAN FD)       |
  | fd     |        |        |        |        |        |        |                              |
  +--------+--------+--------+--------+--------+--------+--------+------------------------------+
  Byte/bit fields:
    byte 0: DLC[7:4], bus[3:1], fd[0]
    bytes 1..4: (addr << 3) | (extended << 2) | (returned << 1) | rejected
    byte 5: checksum = XOR(header[0..4] + payload)
    bytes 6..13 (classic CAN, up to 8 bytes) / bytes 6..69 (CAN FD, up to 64 bytes): payload

  USB/SPI transfer chunking used by this file:
  +--------------------------------------------+   ...   +--------------------------------------------+
  | transport chunk 0                          |         | transport chunk N                          |
  +--------------------------------------------+         +--------------------------------------------+
  | concatenated CANPacket_t bytes             |         | continuation and/or next CANPacket_t bytes |
  | (no per-64-byte counter/header in protocol)|         |                                            |
  +--------------------------------------------+         +--------------------------------------------+

  * comms_can_read outputs this buffer in chunks of a specified length.
    chunks are always the given length, except the last one.
  * comms_can_write reads in this buffer in chunks.
  * both functions maintain an overflow buffer for a partial CANPacket_t that
    spans multiple transfers/chunks.
  * the overflow buffers are reset by a dedicated control transfer handler,
    which is sent by the host on each start of a connection.
*/

typedef struct {
  uint32_t ptr;
  uint32_t tail_size;
  uint8_t data[72];
} asm_buffer;

// CAN wire records have six-byte headers and need not be word-aligned. Use
// CMSIS packed accesses only for SPI paths; never read past len.
void comms_can_wire_copy(uint8_t *dest, const uint8_t *src, uint32_t len, bool batch) {
  #if defined(__UNALIGNED_UINT32_READ) && defined(__UNALIGNED_UINT32_WRITE)
  if (batch) {
    uint32_t pos = 0U;
    while ((len - pos) >= sizeof(uint32_t)) {
      // CMSIS casts its packed-store assignment to void; no assignment value is consumed.
      // cppcheck-suppress misra-c2012-13.4
      __UNALIGNED_UINT32_WRITE(&dest[pos], __UNALIGNED_UINT32_READ(&src[pos]));
      pos += sizeof(uint32_t);
    }
    while (pos < len) {
      dest[pos] = src[pos];
      pos += 1U;
    }
  } else {
    (void)memcpy(dest, src, len);
  }
  #else
  (void)batch;
  (void)memcpy(dest, src, len);
  #endif
}

static asm_buffer can_read_buffer = {.ptr = 0U, .tail_size = 0U};

int comms_can_read_internal(uint8_t *data, uint32_t max_len, bool batch) {
  uint32_t pos = 0U;
  uint32_t spi_read_ptr = 0U;
  uint32_t spi_write_ptr = 0U;
  uint32_t spi_fifo_size = 0U;
  const CANPacket_t *spi_packets = NULL;
  // SPI already runs at the same IRQ priority as CAN/USB. Hold ownership across
  // this capacity-bounded read instead of masking/unmasking for every record.
  if (batch) {
    ENTER_CRITICAL();
    // No producer can change the queue while SPI owns this critical section.
    spi_read_ptr = can_rx_q.r_ptr;
    spi_write_ptr = can_rx_q.w_ptr;
    spi_fifo_size = can_rx_q.fifo_size;
    spi_packets = can_rx_q.elems;
  }

  // Send tail of previous message if it is in buffer
  if (can_read_buffer.ptr > 0U) {
    uint32_t overflow_len = MIN(max_len - pos, can_read_buffer.ptr);
    comms_can_wire_copy(&data[pos], can_read_buffer.data, overflow_len, batch);
    pos += overflow_len;
    comms_can_wire_copy(can_read_buffer.data, &can_read_buffer.data[overflow_len], can_read_buffer.ptr - overflow_len, batch);
    can_read_buffer.ptr -= overflow_len;
  }

  if (can_read_buffer.ptr == 0U) {
    // Fill until capacity or queue exhaustion: hosts use short reads to detect
    // an empty stream. SPI copies only wire bytes, without a full packet pop.
    CANPacket_t can_packet;
    while (pos < max_len) {
      const CANPacket_t *packet = &can_packet;
      bool available;
      if (batch) {
        available = spi_read_ptr != spi_write_ptr;
        packet = &spi_packets[spi_read_ptr];
      } else {
        available = can_pop(&can_rx_q, &can_packet);
      }
      if (available) {
        uint32_t pckt_len = CANPACKET_HEAD_SIZE + dlc_to_len[packet->data_len_code];
        if ((pos + pckt_len) <= max_len) {
          comms_can_wire_copy(&data[pos], (const uint8_t*)packet, pckt_len, batch);
          pos += pckt_len;
        } else {
          comms_can_wire_copy(&data[pos], (const uint8_t*)packet, max_len - pos, batch);
          can_read_buffer.ptr += pckt_len - (max_len - pos);
          // cppcheck-suppress objectIndex
          comms_can_wire_copy(can_read_buffer.data, &((const uint8_t*)packet)[(max_len - pos)], can_read_buffer.ptr, batch);
          pos = max_len;
        }
      }
      if (batch && available) {
        spi_read_ptr = ((spi_read_ptr + 1U) == spi_fifo_size) ? 0U : (spi_read_ptr + 1U);
      }
      if (!available) {
        break;
      }
    }
  }

  if (batch) {
    // Publish once, after all output and carry bytes own their data. Producers
    // may reuse the released slots only after this critical section ends.
    can_rx_q.r_ptr = spi_read_ptr;
    EXIT_CRITICAL();
  }
  return pos;
}

int comms_can_read(uint8_t *data, uint32_t max_len) {
  return comms_can_read_internal(data, max_len, false);
}

int comms_can_read_spi(uint8_t *data, uint32_t max_len) {
  return comms_can_read_internal(data, max_len, true);
}

static asm_buffer can_write_buffer = {.ptr = 0U, .tail_size = 0U};

// send on CAN
void comms_can_write_internal(const uint8_t *data, uint32_t len, uint32_t *pending_packets) {
  uint32_t pos = 0U;

  // Assembling can message with data from buffer
  if (can_write_buffer.ptr != 0U) {
    if (can_write_buffer.tail_size <= (len - pos)) {
      // we have enough data to complete the buffer
      CANPacket_t to_push = {0};
      comms_can_wire_copy(&can_write_buffer.data[can_write_buffer.ptr], &data[pos], can_write_buffer.tail_size, pending_packets != NULL);
      can_write_buffer.ptr += can_write_buffer.tail_size;
      pos += can_write_buffer.tail_size;

      // send out
      comms_can_wire_copy((uint8_t*)&to_push, can_write_buffer.data, can_write_buffer.ptr, pending_packets != NULL);
      can_send_internal(&to_push, to_push.bus, false, pending_packets);

      // reset overflow buffer
      can_write_buffer.ptr = 0U;
      can_write_buffer.tail_size = 0U;
    } else {
      // maybe next time
      uint32_t data_size = len - pos;
      comms_can_wire_copy(&can_write_buffer.data[can_write_buffer.ptr], &data[pos], data_size, pending_packets != NULL);
      can_write_buffer.tail_size -= data_size;
      can_write_buffer.ptr += data_size;
      pos += data_size;
    }
  }

  // rest of the message
  while (pos < len) {
    uint32_t pckt_len = CANPACKET_HEAD_SIZE + dlc_to_len[(data[pos] >> 4U)];
    if ((pos + pckt_len) <= len) {
      CANPacket_t to_push = {0};
      comms_can_wire_copy((uint8_t*)&to_push, &data[pos], pckt_len, pending_packets != NULL);
      can_send_internal(&to_push, to_push.bus, false, pending_packets);
      pos += pckt_len;
    } else {
      comms_can_wire_copy(can_write_buffer.data, &data[pos], len - pos, pending_packets != NULL);
      can_write_buffer.ptr = len - pos;
      can_write_buffer.tail_size = pckt_len - can_write_buffer.ptr;
      pos += can_write_buffer.ptr;
    }
  }

  if (pending_packets == NULL) {
    refresh_can_tx_slots_available();
  }
}

void comms_can_write(const uint8_t *data, uint32_t len) {
  comms_can_write_internal(data, len, NULL);
}

// SPI v3 rejection must leave both queues and the partial-record buffer untouched.
// Preview the byte stream, reserving the worst case (every record either queues
// for TX or immediately produces a safety-rejected receipt). Keep admission and
// execution together so another producer cannot consume the checked slots.
bool comms_can_write_checked(const uint8_t *data, uint32_t len) {
  uint32_t counts[PANDA_CAN_CNT] = {0};
  uint32_t total = 0U;
  uint32_t pos = 0U;
  bool admissible = true;

  ENTER_CRITICAL();
  // Admission needs only the first byte and remaining length of each record.
  // Keep the real partial-record buffer untouched until the entire write fits.
  uint32_t remaining = (can_write_buffer.ptr != 0U) ? can_write_buffer.tail_size : 0U;
  uint8_t record_bus = (can_write_buffer.data[0] >> 1U) & 7U;
  while ((pos < len) && admissible) {
    if (remaining == 0U) {
      remaining = CANPACKET_HEAD_SIZE + dlc_to_len[data[pos] >> 4U];
      record_bus = (data[pos] >> 1U) & 7U;
    }
    uint32_t take = MIN(remaining, len - pos);
    remaining -= take;
    pos += take;
    admissible = record_bus < PANDA_CAN_CNT;
    if (admissible && (remaining == 0U)) {
      counts[record_bus] += 1U;
      total += 1U;
    }
  }
  for (uint32_t bus = 0U; bus < PANDA_CAN_CNT; bus++) {
    admissible = admissible && (counts[bus] <= can_slots_empty(can_queues[bus]));
  }
  admissible = admissible && (total <= can_slots_empty(&can_rx_q));
  if (admissible) {
    uint32_t pending_packets[PANDA_CAN_CNT] = {0};
    comms_can_write_internal(data, len, pending_packets);
    for (uint8_t bus = 0U; bus < PANDA_CAN_CNT; bus++) {
      if (pending_packets[bus] != 0U) {
        process_can_batch(CAN_NUM_FROM_BUS_NUM(bus), pending_packets[bus]);
      }
    }
    refresh_can_tx_slots_available();
  }
  EXIT_CRITICAL();
  return admissible;
}

void comms_can_reset(void) {
  can_write_buffer.ptr = 0U;
  can_write_buffer.tail_size = 0U;
  can_read_buffer.ptr = 0U;
  can_read_buffer.tail_size = 0U;
}

// TODO: make this more general!
void refresh_can_tx_slots_available(void) {
  if (can_tx_check_min_slots_free(MAX_CAN_MSGS_PER_USB_BULK_TRANSFER)) {
    can_tx_comms_resume_usb();
  }
  if (can_tx_check_min_slots_free(MAX_CAN_MSGS_PER_SPI_BULK_TRANSFER)) {
    can_tx_comms_resume_spi();
  }
}
