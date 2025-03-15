#pragma once
// ***************************** Interrupt handlers *****************************

void uart_tx_ring(uart_ring *q);

void USART2_IRQ_Handler(void);

// ***************************** Hardware setup *****************************
void uart_set_baud(USART_TypeDef *u, unsigned int baud);
