#pragma once

struct uart_ring;
typedef struct uart_ring uart_ring;
// This read after reading ISR clears all error interrupts. We don't want compiler warnings, nor optimizations
#define UART_READ_RDR(uart) volatile uint8_t t = (uart)->RDR; UNUSED(t);

void uart_init(uart_ring *q, unsigned int baud);
void uart_tx_ring(uart_ring *q);