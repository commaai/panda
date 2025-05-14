#pragma once

void uart_tx_ring(uart_ring *q);
void uart_set_baud(USART_TypeDef *u, unsigned int baud);
void uart_init(uart_ring *q, int baud);
