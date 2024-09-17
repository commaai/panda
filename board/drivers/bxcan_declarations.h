#pragma once

#include <stdint.h>
#include <stdbool.h>

// IRQs: CAN1_TX, CAN1_RX0, CAN1_SCE
//       CAN2_TX, CAN2_RX0, CAN2_SCE
//       CAN3_TX, CAN3_RX0, CAN3_SCE

#define CAN_ARRAY_SIZE 3
#define CAN_IRQS_ARRAY_SIZE 3
extern CAN_TypeDef *cans[CAN_ARRAY_SIZE];
extern uint8_t can_irq_number[CAN_IRQS_ARRAY_SIZE][CAN_IRQS_ARRAY_SIZE];

bool can_set_speed(uint8_t can_number);
void can_clear_send(CAN_TypeDef *CANx, uint8_t can_number);
void update_can_health_pkt(uint8_t can_number, uint32_t ir_reg);

// ***************************** CAN *****************************
// CANx_SCE IRQ Handler
void can_sce(uint8_t can_number);

// CANx_TX IRQ Handler
void process_can(uint8_t can_number);

// CANx_RX0 IRQ Handler
// blink blue when we are receiving CAN messages
void can_rx(uint8_t can_number);

void CAN1_TX_IRQ_Handler(void);
void CAN1_RX0_IRQ_Handler(void);
void CAN1_SCE_IRQ_Handler(void);

void CAN2_TX_IRQ_Handler(void);
void CAN2_RX0_IRQ_Handler(void);
void CAN2_SCE_IRQ_Handler(void);

void CAN3_TX_IRQ_Handler(void);
void CAN3_RX0_IRQ_Handler(void);
void CAN3_SCE_IRQ_Handler(void);

bool can_init(uint8_t can_number);
