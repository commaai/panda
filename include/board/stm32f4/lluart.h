#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct uart_ring uart_ring;  // Forward decl.
extern uart_ring uart_ring_debug;

void uart_tx_ring(uart_ring *q);
void uart_set_baud(USART_TypeDef *u, unsigned int baud);
void USART2_IRQ_Handler(void);
