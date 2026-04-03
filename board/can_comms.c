#include "board/config.h"
#include "board/utils.h"
#include "board/drivers/can_common.h"
#include "board/can_comms.h"

asm_buffer can_read_buffer = {.ptr = 0U, .tail_size = 0U};
asm_buffer can_write_buffer = {.ptr = 0U, .tail_size = 0U};

int comms_can_read(uint8_t *data, uint32_t max_len) {
  uint32_t pos = 0U;
  if (can_read_buffer.ptr > 0U) {
    uint32_t overflow_len = MIN(max_len - pos, can_read_buffer.ptr);
    (void)memcpy(&data[pos], can_read_buffer.data, overflow_len);
    pos += overflow_len;
    (void)memcpy(can_read_buffer.data, &can_read_buffer.data[overflow_len], can_read_buffer.ptr - overflow_len);
    can_read_buffer.ptr -= overflow_len;
  }
  if (can_read_buffer.ptr == 0U) {
    CANPacket_t can_packet;
    while ((pos < max_len) && can_pop(&can_rx_q, &can_packet)) {
      uint32_t pckt_len = CANPACKET_HEAD_SIZE + dlc_to_len[can_packet.data_len_code];
      if ((pos + pckt_len) <= max_len) {
        (void)memcpy(&data[pos], (uint8_t*)&can_packet, pckt_len);
        pos += pckt_len;
      } else {
        (void)memcpy(&data[pos], (uint8_t*)&can_packet, max_len - pos);
        can_read_buffer.ptr += pckt_len - (max_len - pos);
        (void)memcpy(can_read_buffer.data, &((uint8_t*)&can_packet)[(max_len - pos)], can_read_buffer.ptr);
        pos = max_len;
      }
    }
  }
  return pos;
}

void comms_can_write(const uint8_t *data, uint32_t len) {
  uint32_t pos = 0U;
  if (can_write_buffer.ptr != 0U) {
    if (can_write_buffer.tail_size <= (len - pos)) {
      CANPacket_t to_push = {0};
      (void)memcpy(&can_write_buffer.data[can_write_buffer.ptr], &data[pos], can_write_buffer.tail_size);
      can_write_buffer.ptr += can_write_buffer.tail_size;
      pos += can_write_buffer.tail_size;
      (void)memcpy((uint8_t*)&to_push, can_write_buffer.data, can_write_buffer.ptr);
      can_send(&to_push, to_push.bus, false);
      can_write_buffer.ptr = 0U;
      can_write_buffer.tail_size = 0U;
    } else {
      uint32_t data_size = len - pos;
      (void) memcpy(&can_write_buffer.data[can_write_buffer.ptr], &data[pos], data_size);
      can_write_buffer.tail_size -= data_size;
      can_write_buffer.ptr += data_size;
      pos += data_size;
    }
  }
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

void refresh_can_tx_slots_available(void) {
  if (can_tx_check_min_slots_free(MAX_CAN_MSGS_PER_USB_BULK_TRANSFER)) { can_tx_comms_resume_usb(); }
  if (can_tx_check_min_slots_free(MAX_CAN_MSGS_PER_SPI_BULK_TRANSFER)) { can_tx_comms_resume_spi(); }
}
