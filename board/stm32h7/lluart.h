#ifndef LLUART_H
#define LLUART_H

#include "board/drivers/uart.h"

void uart_rx_ring(uart_ring *q);
void uart_tx_ring(uart_ring *q);
void uart_interrupt_handler(uart_ring *q);
void uart_init(uart_ring *q, unsigned int baud);

#endif
