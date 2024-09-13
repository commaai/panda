#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "config.h"

// IRQs: USART2, USART3, UART5

// ***************************** Definitions *****************************
#define FIFO_SIZE_INT 0x400U

typedef struct uart_ring {
  volatile uint16_t w_ptr_tx;
  volatile uint16_t r_ptr_tx;
  uint8_t *elems_tx;
  uint32_t tx_fifo_size;
  volatile uint16_t w_ptr_rx;
  volatile uint16_t r_ptr_rx;
  uint8_t *elems_rx;
  uint32_t rx_fifo_size;
  USART_TypeDef *uart;
  void (*callback)(struct uart_ring*);
  bool overwrite;
} uart_ring;

#define UART_BUFFER(x, size_rx, size_tx, uart_ptr, callback_ptr, overwrite_mode) \
  uint8_t elems_rx_##x[size_rx]; \
  uint8_t elems_tx_##x[size_tx]; \
  uart_ring uart_ring_##x = {  \
    .w_ptr_tx = 0, \
    .r_ptr_tx = 0, \
    .elems_tx = ((uint8_t *)&(elems_tx_##x)), \
    .tx_fifo_size = (size_tx), \
    .w_ptr_rx = 0, \
    .r_ptr_rx = 0, \
    .elems_rx = ((uint8_t *)&(elems_rx_##x)), \
    .rx_fifo_size = (size_rx), \
    .uart = (uart_ptr), \
    .callback = (callback_ptr), \
    .overwrite = (overwrite_mode) \
  };

// ***************************** Function prototypes *****************************
void debug_ring_callback(uart_ring *ring);
void uart_tx_ring(uart_ring *q);
void uart_send_break(uart_ring *u);

// ******************************** UART buffers ********************************

// debug = USART2
UART_BUFFER(debug, FIFO_SIZE_INT, FIFO_SIZE_INT, USART2, debug_ring_callback, true)

// SOM debug = UART7
#ifdef STM32H7
  UART_BUFFER(som_debug, FIFO_SIZE_INT, FIFO_SIZE_INT, UART7, NULL, true)
#else
  // UART7 is not available on F4
  UART_BUFFER(som_debug, 1U, 1U, NULL, NULL, true)
#endif

uart_ring *get_ring_by_number(int a);

// ************************* Low-level buffer functions *************************
bool get_char(uart_ring *q, char *elem);
bool injectc(uart_ring *q, char elem);
bool put_char(uart_ring *q, char elem);
void clear_uart_buff(uart_ring *q);

// ************************ High-level debug functions **********************
void putch(const char a);
void print(const char *a);
void putui(uint32_t i);
void puthx(uint32_t i, uint8_t len);
void puth(unsigned int i);
void puth2(unsigned int i);
void puth4(unsigned int i);
void hexdump(const void *a, int l);
