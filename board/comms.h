#pragma once

#include <stdint.h>
#include <stdbool.h>

// ======================= CONTROL PACKET =======================

typedef struct {
  uint8_t request;
  uint16_t param1;
  uint16_t param2;
  uint16_t length;
} __attribute__((packed)) ControlPacket_t;

int comms_control_handler(ControlPacket_t *req, uint8_t *resp);
void comms_endpoint2_write(const uint8_t *data, uint32_t len);

// ======================= CAN COMMS =======================

/*
  CAN transactions to and from the host come in the form of
  a certain number of CANPacket_t. The transaction is split
  into multiple transfers or chunks.

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

int comms_can_read(uint8_t *data, uint32_t max_len);

// send on CAN
void comms_can_write(const uint8_t *data, uint32_t len);
void comms_can_reset(void);

void refresh_can_tx_slots_available(void);

// ======================= MAIN COMMS =======================

void set_safety_mode(uint16_t mode, uint16_t param);
bool is_car_safety_mode(uint16_t mode);
