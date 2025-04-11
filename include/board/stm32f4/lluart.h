#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "drivers/uart.h"
#include "platform_definitions.h"

void uart_tx_ring(uart_ring *q);
void uart_set_baud(USART_TypeDef *u, unsigned int baud);
