#pragma once

#include <stdint.h>

#include "board/can.h"
#include "board/config.h"

#define CAN_ACK_ERROR 3U

typedef struct {
  volatile uint32_t header[2];
  volatile uint32_t data_word[CANPACKET_DATA_SIZE_MAX/4U];
} canfd_fifo;

extern FDCAN_GlobalTypeDef *cans[PANDA_CAN_CNT];

void can_clear_send(FDCAN_GlobalTypeDef *FDCANx, uint8_t can_number);
void update_can_health_pkt(uint8_t can_number, uint32_t ir_reg);

// ***************************** CAN *****************************
// FDFDCANx_IT1 IRQ Handler (TX)
void process_can(uint8_t can_number);
bool can_init(uint8_t can_number);