#ifndef CAN_COMMON_H
#define CAN_COMMON_H

#include "board/drivers/driver_declarations.h"

extern uint32_t safety_tx_blocked;
extern uint32_t safety_rx_invalid;
extern uint32_t tx_buffer_overflow;
extern uint32_t rx_buffer_overflow;

extern can_health_t can_health[PANDA_CAN_CNT];
extern bool ignition_can;
extern uint32_t ignition_can_cnt;
extern bool can_silent;
extern bool can_loopback;

#define CAN_RX_BUFFER_SIZE 4096U
#define CAN_TX_BUFFER_SIZE 416U

extern can_ring can_rx_q;
extern can_ring can_tx1_q;
extern can_ring can_tx2_q;
extern can_ring can_tx3_q;
extern can_ring *can_queues[PANDA_CAN_CNT];

extern bus_config_t bus_config[PANDA_CAN_CNT];

#define CANIF_FROM_CAN_NUM(x) (cans[(x)])
#define BUS_NUM_FROM_CAN_NUM(can_num) (bus_config[(can_num)].bus_lookup)
#define CAN_NUM_FROM_BUS_NUM(bus_num) (bus_config[(bus_num)].can_num_lookup)

bool can_pop(can_ring *q, CANPacket_t *elem);
bool can_push(can_ring *q, const CANPacket_t *elem);
uint32_t can_slots_empty(const can_ring *q);
void can_clear(can_ring *q);
void can_init_all(void);
void can_set_orientation(bool flipped);
#ifdef PANDA_JUNGLE
void can_set_forwarding(uint8_t from, uint8_t to);
#endif
void ignition_can_hook(CANPacket_t *msg);
bool can_tx_check_min_slots_free(uint32_t min);
void can_set_checksum(CANPacket_t *packet);
bool can_check_checksum(CANPacket_t *packet);
void can_send(CANPacket_t *to_push, uint8_t bus_number, bool skip_tx_hook);
bool is_speed_valid(uint32_t speed, const uint32_t *all_speeds, uint8_t len);

#endif
