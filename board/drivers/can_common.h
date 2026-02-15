#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "board/can.h"

typedef struct {
  volatile uint32_t w_ptr;
  volatile uint32_t r_ptr;
  uint32_t fifo_size;
  CANPacket_t *elems;
} can_ring;

typedef struct {
  uint8_t bus_lookup;
  uint8_t can_num_lookup;
  int8_t forwarding_bus;
  uint32_t can_speed;
  uint32_t can_data_speed;
  bool canfd_auto;
  bool canfd_enabled;
  bool brs_enabled;
  bool canfd_non_iso;
} bus_config_t;

// External variables
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

extern can_ring *can_queues[PANDA_CAN_CNT];
extern bus_config_t bus_config[PANDA_CAN_CNT];

// External ring buffers
extern can_ring can_rx_q;
extern can_ring can_tx1_q;
extern can_ring can_tx2_q;
extern can_ring can_tx3_q;

// Helper macros
#define WORD_TO_BYTE_ARRAY(dst8, src32) 0[dst8] = ((src32) & 0xFFU); 1[dst8] = (((src32) >> 8U) & 0xFFU); 2[dst8] = (((src32) >> 16U) & 0xFFU); 3[dst8] = (((src32) >> 24U) & 0xFFU)
#define BYTE_ARRAY_TO_WORD(dst32, src8) ((dst32) = 0[src8] | (1[src8] << 8U) | (2[src8] << 16U) | (3[src8] << 24U))

#define CANIF_FROM_CAN_NUM(num) (cans[num])
#define BUS_NUM_FROM_CAN_NUM(num) (bus_config[num].bus_lookup)
#define CAN_NUM_FROM_BUS_NUM(num) (bus_config[num].can_num_lookup)

// Function prototypes
bool can_init(uint8_t can_number);
void process_can(uint8_t can_number);

// Interrupt safe queue functions
bool can_pop(can_ring *q, CANPacket_t *elem);
bool can_push(can_ring *q, const CANPacket_t *elem);
uint32_t can_slots_empty(const can_ring *q);
void can_clear(can_ring *q);

// Initialization and configuration
void can_init_all(void);
void can_set_orientation(bool flipped);
#ifdef PANDA_JUNGLE
void can_set_forwarding(uint8_t from, uint8_t to);
#endif

// CAN message handling
void ignition_can_hook(CANPacket_t *to_push);
bool can_tx_check_min_slots_free(uint32_t min);
uint8_t calculate_checksum(const uint8_t *dat, uint32_t len);
void can_set_checksum(CANPacket_t *packet);
bool can_check_checksum(CANPacket_t *packet);
void can_send(CANPacket_t *to_push, uint8_t bus_number, bool skip_tx_hook);
bool is_speed_valid(uint32_t speed, const uint32_t *all_speeds, uint8_t len);

// External dependencies (implemented elsewhere)
extern void refresh_can_tx_slots_available(void);
extern int safety_tx_hook(CANPacket_t *to_send);
