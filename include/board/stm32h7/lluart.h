#pragma once
#include "drivers/uart.h"
#include "drivers/interrupts.h"

// Function declarations
void uart_tx_ring(uart_ring *q);
void uart_set_baud(USART_TypeDef *u, unsigned int baud);
void uart_init(uart_ring *q, int baud);

// This read after reading ISR clears all error interrupts. We don't want compiler warnings, nor optimizations
#define UART_READ_RDR(uart) volatile uint8_t t = (uart)->RDR; UNUSED(t);