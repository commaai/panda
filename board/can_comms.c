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

#include <stdbool.h>
#include <stdint.h>

#include "opendbc/safety/can.h"

#include "board/can.h"
#include "board/can_comms.h"
#include "board/comms_constants.h"

void *memcpy(void *dest, const void *src, unsigned int len);

typedef struct {
  volatile uint32_t w_ptr;
  volatile uint32_t r_ptr;
  uint32_t fifo_size;
  CANPacket_t *elems;
} can_ring;

extern can_ring can_rx_q;

bool can_pop(can_ring *q, CANPacket_t *elem);
void can_send(CANPacket_t *to_push, uint8_t bus_number, bool skip_tx_hook);
bool can_tx_check_min_slots_free(uint32_t min);
void can_tx_comms_resume_usb(void);
void can_tx_comms_resume_spi(void);

typedef struct {
  uint32_t ptr;
  uint32_t tail_size;
  uint8_t data[sizeof(CANPacket_t)];
} asm_buffer;

static uint32_t min_u32(uint32_t a, uint32_t b) {
  return (a < b) ? a : b;
}

static asm_buffer can_read_buffer = {.ptr = 0U, .tail_size = 0U};

int comms_can_read(uint8_t *data, uint32_t max_len) {
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
    while ((pos < max_len) && can_pop(&can_rx_q, &can_packet)) {
      uint32_t pckt_len = CANPACKET_HEAD_SIZE + dlc_to_len[can_packet.data_len_code];
      if ((pos + pckt_len) <= max_len) {
        (void)memcpy(&data[pos], (uint8_t*)&can_packet, pckt_len);
        pos += pckt_len;
      } else {
        (void)memcpy(&data[pos], (uint8_t*)&can_packet, max_len - pos);
        can_read_buffer.ptr += pckt_len - (max_len - pos);
        // cppcheck-suppress objectIndex
        (void)memcpy(can_read_buffer.data, &((uint8_t*)&can_packet)[(max_len - pos)], can_read_buffer.ptr);
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
      CANPacket_t to_push = {0};
      (void)memcpy(&can_write_buffer.data[can_write_buffer.ptr], &data[pos], can_write_buffer.tail_size);
      can_write_buffer.ptr += can_write_buffer.tail_size;
      pos += can_write_buffer.tail_size;

      // send out
      (void)memcpy((uint8_t*)&to_push, can_write_buffer.data, can_write_buffer.ptr);
      can_send(&to_push, to_push.bus, false);

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
    uint32_t pckt_len = CANPACKET_HEAD_SIZE + dlc_to_len[(data[pos] >> 4U)];
    if ((pos + pckt_len) <= len) {
      CANPacket_t to_push = {0};
      (void)memcpy((uint8_t*)&to_push, &data[pos], pckt_len);
      can_send(&to_push, to_push.bus, false);
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
void refresh_can_tx_slots_available(void) {
  if (can_tx_check_min_slots_free(MAX_CAN_MSGS_PER_USB_BULK_TRANSFER)) {
    can_tx_comms_resume_usb();
  }
  if (can_tx_check_min_slots_free(MAX_CAN_MSGS_PER_SPI_BULK_TRANSFER)) {
    can_tx_comms_resume_spi();
  }
}
