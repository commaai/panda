#pragma once

void uart_tx_ring(uart_ring *q);
void uart_init(uart_ring *q, unsigned int baud);
