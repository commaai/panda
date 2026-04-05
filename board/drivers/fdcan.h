#ifndef FDCAN_H
#define FDCAN_H

#include "board/drivers/drivers.h"

#define CAN_ACK_ERROR 3U

// cppcheck-suppress misra-c2012-2.3 ; used in driver implementations
// cppcheck-suppress misra-c2012-2.4 ; used in driver implementations
typedef struct {
  uint32_t header[2];
  uint32_t data_word[16];
} canfd_fifo;

extern FDCAN_GlobalTypeDef *cans[PANDA_CAN_CNT];

void can_clear_send(FDCAN_GlobalTypeDef *FDCANx, uint8_t can_number);
void update_can_health_pkt(uint8_t can_number, uint32_t ir_reg);
void process_can(uint8_t can_number);
void can_rx(uint8_t can_number);
bool can_init(uint8_t can_number);

#endif
