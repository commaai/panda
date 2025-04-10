#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "can.h"
#include "health.h"

// TODO: Dedup this.
#include "safety/board/drivers/can_common.h"

typedef struct {
  volatile uint32_t w_ptr;
  volatile uint32_t r_ptr;
  uint32_t fifo_size;
  CANPacket_t *elems;
} can_ring;

#define can_buffer(x, size) \
  static CANPacket_t elems_##x[size]; \
  can_ring can_##x = { .w_ptr = 0, .r_ptr = 0, .fifo_size = (size), .elems = (CANPacket_t *)&(elems_##x) };

#define CAN_RX_BUFFER_SIZE 4096U
#define CAN_TX_BUFFER_SIZE 416U

extern can_ring can_rx_q;
extern can_ring can_tx1_q;
extern can_ring can_tx2_q;
extern can_ring can_tx3_q;

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

extern uint32_t safety_tx_blocked;
extern uint32_t safety_rx_invalid;
extern uint32_t tx_buffer_overflow;
extern uint32_t rx_buffer_overflow;

#define CAN_HEALTH_ARRAY_SIZE 3
extern can_health_t can_health[CAN_HEALTH_ARRAY_SIZE];

// Ignition detected from CAN meessages
extern bool ignition_can;
extern uint32_t ignition_can_cnt;

#define ALL_CAN_SILENT 0xFF
#define ALL_CAN_LIVE 0

extern int can_live;
extern int pending_can_live;
extern int can_silent;
extern bool can_loopback;

// ******************* functions prototypes *********************
bool can_init(uint8_t can_number);
void process_can(uint8_t can_number);

// ********************* instantiate queues *********************
#define CAN_QUEUES_ARRAY_SIZE 3
extern can_ring *can_queues[CAN_QUEUES_ARRAY_SIZE];

// helpers
#define WORD_TO_BYTE_ARRAY(dst8, src32) 0[dst8] = ((src32) & 0xFFU); 1[dst8] = (((src32) >> 8U) & 0xFFU); 2[dst8] = (((src32) >> 16U) & 0xFFU); 3[dst8] = (((src32) >> 24U) & 0xFFU)
#define BYTE_ARRAY_TO_WORD(dst32, src8) ((dst32) = 0[src8] | (1[src8] << 8U) | (2[src8] << 16U) | (3[src8] << 24U))

// ********************* interrupt safe queue *********************
bool can_pop(can_ring *q, CANPacket_t *elem);
bool can_push(can_ring *q, const CANPacket_t *elem);
uint32_t can_slots_empty(const can_ring *q);

// assign CAN numbering
// bus num: CAN Bus numbers in panda, sent to/from USB
//    Min: 0; Max: 127; Bit 7 marks message as receipt (bus 129 is receipt for but 1)
// cans: Look up MCU can interface from bus number
// can number: numeric lookup for MCU CAN interfaces (0 = CAN1, 1 = CAN2, etc);
// bus_lookup: Translates from 'can number' to 'bus number'.
// can_num_lookup: Translates from 'bus number' to 'can number'.
// forwarding bus: If >= 0, forward all messages from this bus to the specified bus.

// Helpers
// Panda:       Bus 0=CAN1   Bus 1=CAN2   Bus 2=CAN3
#define BUS_CONFIG_ARRAY_SIZE 4
extern bus_config_t bus_config[BUS_CONFIG_ARRAY_SIZE];

#define CANIF_FROM_CAN_NUM(num) (cans[num])
#define BUS_NUM_FROM_CAN_NUM(num) (bus_config[num].bus_lookup)
#define CAN_NUM_FROM_BUS_NUM(num) (bus_config[num].can_num_lookup)

void can_init_all(void);
void can_clear(can_ring *q);
void can_set_orientation(bool flipped);
#ifdef PANDA_JUNGLE
void can_set_forwarding(uint8_t from, uint8_t to);
#endif
void ignition_can_hook(CANPacket_t *to_push);
bool can_tx_check_min_slots_free(uint32_t min);
void can_send(CANPacket_t *to_push, uint8_t bus_number, bool skip_tx_hook);
bool is_speed_valid(uint32_t speed, const uint32_t *all_speeds, uint8_t len);

extern bool safety_tx_hook(CANPacket_t *to_send);
