// UART driver interface
#pragma once

#include "board/drivers/drivers.h"

// Definitions
#define UART_BUFFER(x, size_rx, size_tx, uart_ptr, callback_ptr, overwrite_mode) \
  extern uart_ring uart_ring_##x; \
  UART_BUFFER_DEF_##x(uart_ring_##x);

// Low-level buffer functions prototypes
bool get_char(uart_ring *q, char *elem);
bool injectc(uart_ring *q, char elem);
bool put_char(uart_ring *q, char elem);

// High-level debug function prototypes
void putch(const char a);
void print(const char *a);
void puthx(uint32_t i, uint8_t len);
void puth(unsigned int i);

#if defined(DEBUG_SPI) || defined(BOOTSTUB) || defined(DEBUG)
void puth4(unsigned int i);
#endif

#if defined(DEBUG_SPI) || defined(BOOTSTUB) || defined(DEBUG_USB) || defined(DEBUG_COMMS)
void hexdump(const void *a, int l);
#endif

uart_ring *get_ring_by_number(int a);
