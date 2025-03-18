#pragma once
#include "drivers/uart.h"
#include "drivers/interrupts.h"

// Function declarations
void uart_tx_ring(uart_ring *q);
void uart_set_baud(USART_TypeDef *u, unsigned int baud);

// ***************************** Hardware setup *****************************

#define DIV_(_PCLK_, _BAUD_)                    (((_PCLK_) * 25U) / (4U * (_BAUD_)))
#define DIVMANT_(_PCLK_, _BAUD_)                (DIV_((_PCLK_), (_BAUD_)) / 100U)
#define DIVFRAQ_(_PCLK_, _BAUD_)                ((((DIV_((_PCLK_), (_BAUD_)) - (DIVMANT_((_PCLK_), (_BAUD_)) * 100U)) * 16U) + 50U) / 100U)
#define USART_BRR_(_PCLK_, _BAUD_)              ((DIVMANT_((_PCLK_), (_BAUD_)) << 4) | (DIVFRAQ_((_PCLK_), (_BAUD_)) & 0x0FU))
