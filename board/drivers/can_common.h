#pragma once

#include "board/drivers/drivers.h"

// ********************* instantiate queues *********************
#define can_buffer(x, size) \
  static CANPacket_t elems_##x[size]; \
  extern can_ring can_##x; \
  can_ring can_##x = { .w_ptr = 0, .r_ptr = 0, .fifo_size = (size), .elems = (CANPacket_t *)&(elems_##x) };

#define CAN_RX_BUFFER_SIZE 4096U
#define CAN_TX_BUFFER_SIZE 416U

extern uint32_t safety_tx_blocked;
extern uint32_t safety_rx_invalid;
extern uint32_t tx_buffer_overflow;
extern uint32_t rx_buffer_overflow;

extern can_health_t can_health[PANDA_CAN_CNT];

// Ignition detected from CAN meessages
extern bool ignition_can;
extern uint32_t ignition_can_cnt;

extern bool can_silent;
extern bool can_loopback;

extern can_ring can_rx_q;
extern can_ring can_tx1_q;
extern can_ring can_tx2_q;
extern can_ring can_tx3_q;

extern can_ring *can_queues[PANDA_CAN_CNT];

// ********************* interrupt safe queue *********************
bool can_pop(can_ring *q, CANPacket_t *elem);
bool can_push(can_ring *q, const CANPacket_t *elem);
uint32_t can_slots_empty(const can_ring *q);
void can_clear(can_ring *q);

extern bus_config_t bus_config[PANDA_CAN_CNT];

void can_init_all(void);
void can_set_orientation(bool flipped);
#ifdef PANDA_JUNGLE
void can_set_forwarding(uint8_t from, uint8_t to);
#endif
void ignition_can_hook(CANPacket_t *to_push);
bool can_tx_check_min_slots_free(uint32_t min);
uint8_t calculate_checksum(const uint8_t *dat, uint32_t len);
void can_set_checksum(CANPacket_t *packet);
bool can_check_checksum(CANPacket_t *packet);
void can_send(CANPacket_t *to_push, uint8_t bus_number, bool skip_tx_hook);
bool is_speed_valid(uint32_t speed, const uint32_t *all_speeds, uint8_t len);
