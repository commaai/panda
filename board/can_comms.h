#ifndef BOARD_CAN_COMMS_H
#define BOARD_CAN_COMMS_H

/*
  CAN transactions to and from the host come in the form of
  a certain number of CAN packets in the wire format of can_wire.h.
  The transaction is split into multiple transfers or chunks.

  USB/SPI transfer chunking used by this file:
  +--------------------------------------------+   ...   +--------------------------------------------+
  | transport chunk 0                          |         | transport chunk N                          |
  +--------------------------------------------+         +--------------------------------------------+
  | concatenated CAN packets                   |         | continuation and/or next CAN packets       |
  | (no per-64-byte counter/header in protocol)|         |                                            |
  +--------------------------------------------+         +--------------------------------------------+

  * comms_can_read outputs this buffer in chunks of a specified length.
    chunks are always the given length, except the last one.
  * comms_can_write reads in this buffer in chunks.
  * both functions maintain an overflow buffer for a partial CAN packet that
    spans multiple transfers/chunks.
  * the overflow buffers are reset by a dedicated control transfer handler,
    which is sent by the host on each start of a connection.
*/

#include "board/can_wire.h"

typedef struct {
  uint32_t ptr;
  uint32_t tail_size;
  uint8_t data[CANPACKET_HEAD_SIZE + CANPACKET_DATA_SIZE_MAX];
} asm_buffer;

static void comms_can_send(const uint8_t *buf) {
  CANPacket_t to_push = {0};
  bool checksum_valid = can_packet_deserialize(buf, &to_push);
  if (!checksum_valid && (to_push.bus < PANDA_CAN_CNT)) {
    can_health[CAN_NUM_FROM_BUS_NUM(to_push.bus)].total_tx_checksum_error_cnt += 1U;
  }
  // a corrupt packet still goes through safety_tx_hook and is returned to the host if the hook
  // rejects it, but it's never queued: can_send drops packets for a bus that doesn't exist
  can_send(&to_push, checksum_valid ? to_push.bus : PANDA_CAN_CNT, false);
}

static asm_buffer can_read_buffer = {.ptr = 0U, .tail_size = 0U};

uint32_t comms_can_read(uint8_t *data, uint32_t max_len) {
  uint32_t pos = 0U;

  // Send tail of previous message if it is in buffer
  if (can_read_buffer.ptr > 0U) {
    uint32_t overflow_len = min_u32(max_len - pos, can_read_buffer.ptr);
    (void)memcpy(&data[pos], can_read_buffer.data, overflow_len);
    pos += overflow_len;
    (void)memcpy(can_read_buffer.data, &can_read_buffer.data[overflow_len], can_read_buffer.ptr - overflow_len);
    can_read_buffer.ptr -= overflow_len;
  }

  if (can_read_buffer.ptr == 0U) {
    // Fill rest of buffer with new data
    CANPacket_t can_packet;
    while (pos < max_len) {
      if (!can_pop(&can_rx_q, &can_packet)) {
        break;
      }
      uint32_t pckt_len = CANPACKET_HEAD_SIZE + GET_LEN(&can_packet);
      if ((pos + pckt_len) <= max_len) {
        can_packet_serialize(&can_packet, &data[pos]);
        pos += pckt_len;
      } else {
        uint8_t pckt[CANPACKET_HEAD_SIZE + CANPACKET_DATA_SIZE_MAX];
        can_packet_serialize(&can_packet, pckt);
        (void)memcpy(&data[pos], pckt, max_len - pos);
        can_read_buffer.ptr += pckt_len - (max_len - pos);
        (void)memcpy(can_read_buffer.data, &pckt[(max_len - pos)], can_read_buffer.ptr);
        pos = max_len;
      }
    }
  }

  return pos;
}

static asm_buffer can_write_buffer = {.ptr = 0U, .tail_size = 0U};

// send on CAN
void comms_can_write(const uint8_t *data, uint32_t len) {
  uint32_t pos = 0U;

  // Assembling can message with data from buffer
  if (can_write_buffer.ptr != 0U) {
    if (can_write_buffer.tail_size <= (len - pos)) {
      // we have enough data to complete the buffer
      (void)memcpy(&can_write_buffer.data[can_write_buffer.ptr], &data[pos], can_write_buffer.tail_size);
      can_write_buffer.ptr += can_write_buffer.tail_size;
      pos += can_write_buffer.tail_size;

      // send out
      comms_can_send(can_write_buffer.data);

      // reset overflow buffer
      can_write_buffer.ptr = 0U;
      can_write_buffer.tail_size = 0U;
    } else {
      // maybe next time
      uint32_t data_size = len - pos;
      (void) memcpy(&can_write_buffer.data[can_write_buffer.ptr], &data[pos], data_size);
      can_write_buffer.tail_size -= data_size;
      can_write_buffer.ptr += data_size;
      pos += data_size;
    }
  }

  // rest of the message
  while (pos < len) {
    uint32_t pckt_len = CANPACKET_HEAD_SIZE + (uint32_t)dlc_to_len[(data[pos] >> 4U)];
    if ((pos + pckt_len) <= len) {
      comms_can_send(&data[pos]);
      pos += pckt_len;
    } else {
      (void)memcpy(can_write_buffer.data, &data[pos], len - pos);
      can_write_buffer.ptr = len - pos;
      can_write_buffer.tail_size = pckt_len - can_write_buffer.ptr;
      pos += can_write_buffer.ptr;
    }
  }

  refresh_can_tx_slots_available();
}

void comms_can_reset(void) {
  can_write_buffer.ptr = 0U;
  can_write_buffer.tail_size = 0U;
  can_read_buffer.ptr = 0U;
  can_read_buffer.tail_size = 0U;
}

// TODO: make this more general!
static void refresh_can_tx_slots_available(void) {
  if (can_tx_check_min_slots_free(MAX_CAN_MSGS_PER_USB_BULK_TRANSFER)) {
    can_tx_comms_resume_usb();
  }
  if (can_tx_check_min_slots_free(MAX_CAN_MSGS_PER_SPI_BULK_TRANSFER)) {
    can_tx_comms_resume_spi();
  }
}

#endif  // BOARD_CAN_COMMS_H
