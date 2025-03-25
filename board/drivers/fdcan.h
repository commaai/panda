#pragma once
#include "board/stm32h7/stm32h7_config.h"
// IRQs: FDCAN1_IT0, FDCAN1_IT1
//       FDCAN2_IT0, FDCAN2_IT1
//       FDCAN3_IT0, FDCAN3_IT1

#define CANFD

#define CANS_ARRAY_SIZE 3
extern FDCAN_GlobalTypeDef *cans[CANS_ARRAY_SIZE];


void can_clear_send(FDCAN_GlobalTypeDef *FDCANx, uint8_t can_number);
void update_can_health_pkt(uint8_t can_number, uint32_t ir_reg);

// ***************************** CAN *****************************
// FDFDCANx_IT1 IRQ Handler (TX)
void process_can(uint8_t can_number);

bool can_init(uint8_t can_number);
