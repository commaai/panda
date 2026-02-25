#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct board board;
typedef struct harness_configuration harness_configuration;
typedef struct uart_ring uart_ring;

extern uint8_t hw_type;
extern board *current_board;

extern uart_ring uart_ring_som_debug;

void print(const char *a);
void puth(unsigned int i);
void puth2(unsigned int i);
void puth4(unsigned int i);
void puthx(uint32_t i, uint8_t len);
void hexdump(const void *a, int l);
void putch(const char a);
void uart_init(uart_ring *q, unsigned int baud);
