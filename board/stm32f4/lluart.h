#pragma once
// ***************************** Interrupt handlers *****************************
#include "board/drivers/uart.h"
#include "config.h"

typedef struct uart_ring uart_ring;

void uart_tx_ring(uart_ring *q);

void USART2_IRQ_Handler(void);

// ***************************** Hardware setup *****************************
void uart_set_baud(USART_TypeDef *u, unsigned int baud);
