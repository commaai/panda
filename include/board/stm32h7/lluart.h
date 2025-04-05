#pragma once
#include "platform_definitions.h"

typedef struct uart_ring uart_ring;  // Forward decl.

void uart_tx_ring(uart_ring *q);
void uart_set_baud(USART_TypeDef *u, unsigned int baud);
void uart_init(uart_ring *q, int baud);

extern uart_ring uart_ring_som_debug;
