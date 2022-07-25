typedef struct {
  int ptr;
  int tail_size;
  uint8_t data[72];
  uint8_t counter;
} asm_buffer;

asm_buffer can_read_buffer = {.ptr = 0, .tail_size = 0, .counter = 0};
int total_rx_size = 0;

int comms_can_read(uint8_t *data, uint32_t max_len) {
  uint32_t pos = 1;
  data[0] = can_read_buffer.counter;
  // Send tail of previous message if it is in buffer
  if (can_read_buffer.ptr > 0) {
    if (can_read_buffer.ptr <= 63) {
      (void)memcpy(&data[pos], can_read_buffer.data, can_read_buffer.ptr);
      pos += can_read_buffer.ptr;
      can_read_buffer.ptr = 0;
    } else {
      (void)memcpy(&data[pos], can_read_buffer.data, 63);
      can_read_buffer.ptr = can_read_buffer.ptr - 63;
      (void)memcpy(can_read_buffer.data, &can_read_buffer.data[63], can_read_buffer.ptr);
      pos += 63;
    }
  }

  if (total_rx_size > MAX_EP1_CHUNK_PER_BULK_TRANSFER) {
    total_rx_size = 0;
    can_read_buffer.counter = 0U;
  } else {
    CANPacket_t can_packet;
    while ((pos < max_len) && can_pop(&can_rx_q, &can_packet)) {
      int pckt_len = CANPACKET_HEAD_SIZE + dlc_to_len[can_packet.data_len_code];
      if ((pos + pckt_len) <= max_len) {
        (void)memcpy(&data[pos], &can_packet, pckt_len);
        pos += pckt_len;
      } else {
        (void)memcpy(&data[pos], &can_packet, max_len - pos);
        can_read_buffer.ptr = pckt_len - (max_len - pos);
        // cppcheck-suppress objectIndex
        (void)memcpy(can_read_buffer.data, &((uint8_t*)&can_packet)[(max_len - pos)], can_read_buffer.ptr);
        pos = max_len;
      }
    }
    can_read_buffer.counter++;
    total_rx_size += pos;
  }
  if (pos != max_len) {
    can_read_buffer.counter = 0U;
    total_rx_size = 0;
  }
  if (pos <= 1) { pos = 0; }
  return pos;
}

asm_buffer can_write_buffer = {.ptr = 0, .tail_size = 0, .counter = 0};

// send on CAN
void comms_can_write(uint8_t *data, uint32_t len) {
  // Got first packet from a stream, resetting buffer and counter
  if (data[0] == 0U) {
    can_write_buffer.counter = 0U;
    can_write_buffer.ptr = 0;
    can_write_buffer.tail_size = 0;
  }
  // Assembling can message with data from buffer
  if (data[0] == can_write_buffer.counter) {
    uint32_t pos = 1;
    can_write_buffer.counter++;
    if (can_write_buffer.ptr != 0) {
      if (can_write_buffer.tail_size <= 63) {
        CANPacket_t to_push;
        (void)memcpy(&can_write_buffer.data[can_write_buffer.ptr], &data[pos], can_write_buffer.tail_size);
        (void)memcpy(&to_push, can_write_buffer.data, can_write_buffer.ptr + can_write_buffer.tail_size);
        can_send(&to_push, to_push.bus, false);
        pos += can_write_buffer.tail_size;
        can_write_buffer.ptr = 0;
        can_write_buffer.tail_size = 0;
      } else {
        (void)memcpy(&can_write_buffer.data[can_write_buffer.ptr], &data[pos], len - pos);
        can_write_buffer.tail_size -= 63;
        can_write_buffer.ptr += 63;
        pos += 63;
      }
    }

    while (pos < len) {
      int pckt_len = CANPACKET_HEAD_SIZE + dlc_to_len[(data[pos] >> 4U)];
      if ((pos + pckt_len) <= len) {
        CANPacket_t to_push;
        (void)memcpy(&to_push, &data[pos], pckt_len);
        can_send(&to_push, to_push.bus, false);
        pos += pckt_len;
      } else {
        (void)memcpy(can_write_buffer.data, &data[pos], len - pos);
        can_write_buffer.ptr = len - pos;
        can_write_buffer.tail_size = pckt_len - can_write_buffer.ptr;
        pos += can_write_buffer.ptr;
      }
    }
  }
}
