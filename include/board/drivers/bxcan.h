#pragma once
#include <stdbool.h>

#include "platform_definitions.h"
#include "drivers/can_common.h"

// IRQs: CAN1_TX, CAN1_RX0, CAN1_SCE
//       CAN2_TX, CAN2_RX0, CAN2_SCE
//       CAN3_TX, CAN3_RX0, CAN3_SCE

#define CAN_ARRAY_SIZE 3
#define CAN_IRQS_ARRAY_SIZE 3
extern CAN_TypeDef *cans[CAN_ARRAY_SIZE];
extern uint8_t can_irq_number[CAN_IRQS_ARRAY_SIZE][CAN_IRQS_ARRAY_SIZE];

bool can_set_speed(uint8_t can_number);
void can_clear_send(CAN_TypeDef *CANx, uint8_t can_number);

// ***************************** CAN *****************************
// CANx_TX IRQ Handler
void process_can(uint8_t can_number);
// CANx_RX0 IRQ Handler
// blink blue when we are receiving CAN messages
void can_rx(uint8_t can_number);
bool can_init(uint8_t can_number);

extern int safety_fwd_hook(int bus_num, int addr);
extern bool safety_rx_hook(const CANPacket_t *to_push);
