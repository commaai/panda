#ifndef BOARD_CAN_WIRE_H
#define BOARD_CAN_WIRE_H

/*
  CAN packet byte layout (wire format used by comms_can_{read,write}):
  +--------+--------+--------+--------+--------+--------+--------+------------------------------+
  | byte 0 | byte 1 | byte 2 | byte 3 | byte 4 | byte 5 | byte 6 | ... byte 13 / byte 69        |
  +--------+--------+--------+--------+--------+--------+--------+------------------------------+
  | DLC    | addr   | addr   | addr   | addr   | cksum  | data0  | ... data7 / data63           |
  | bus    | flags  |        |        |        |        |        | (classic CAN / CAN FD)       |
  | fd     |        |        |        |        |        |        |                              |
  +--------+--------+--------+--------+--------+--------+--------+------------------------------+
  Byte/bit fields:
    byte 0: DLC[7:4], bus[3:1], fd[0]
    bytes 1..4: little-endian (addr << 3) | (extended << 2) | (returned << 1) | rejected
    byte 5: checksum = XOR(header[0..4] + payload)
    bytes 6..13 (classic CAN, up to 8 bytes) / bytes 6..69 (CAN FD, up to 64 bytes): payload

  CAN_PACKET_VERSION is a hash of this file.
*/

#define CANPACKET_HEAD_SIZE 6U

// Writes the packet in the wire format, CANPACKET_HEAD_SIZE + GET_LEN(packet) bytes
static void can_packet_serialize(const CANPacket_t *packet, uint8_t *buf) {
  uint32_t addr_word = ((uint32_t)packet->addr << 3U) | ((uint32_t)packet->extended << 2U) | ((uint32_t)packet->returned << 1U) | (uint32_t)packet->rejected;

  buf[0] = (packet->data_len_code << 4U) | (packet->bus << 1U) | packet->fd;
  WORD_TO_BYTE_ARRAY(&buf[1], addr_word);
  uint8_t checksum = buf[0] ^ buf[1] ^ buf[2] ^ buf[3] ^ buf[4];
  uint32_t len = GET_LEN(packet);
  for (uint32_t i = 0U; i < len; i++) {
    uint8_t byte = packet->data[i];
    buf[CANPACKET_HEAD_SIZE + i] = byte;
    checksum ^= byte;
  }
  buf[5] = checksum;
}

// Reads a packet in the wire format, returns whether its checksum is valid
static bool can_packet_deserialize(const uint8_t *buf, CANPacket_t *packet) {
  uint32_t addr_word;
  BYTE_ARRAY_TO_WORD(addr_word, &buf[1]);

  packet->fd = buf[0] & 0x1U;
  packet->bus = (buf[0] >> 1U) & 0x7U;
  packet->data_len_code = buf[0] >> 4U;
  packet->rejected = buf[1] & 0x1U;
  packet->returned = (buf[1] >> 1U) & 0x1U;
  packet->extended = (buf[1] >> 2U) & 0x1U;
  packet->addr = addr_word >> 3U;

  uint8_t checksum = buf[0] ^ buf[1] ^ buf[2] ^ buf[3] ^ buf[4] ^ buf[5];
  uint32_t len = GET_LEN(packet);
  for (uint32_t i = 0U; i < len; i++) {
    uint8_t byte = buf[CANPACKET_HEAD_SIZE + i];
    packet->data[i] = byte;
    checksum ^= byte;
  }
  return checksum == 0U;
}

#endif  // BOARD_CAN_WIRE_H
