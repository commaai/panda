#pragma once

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

// ***************************** Function prototypes *****************************
void debug_ring_callback(uart_ring *ring);
void clear_uart_buff(uart_ring *q);

// Platform-specific function - implemented in platform headers
void uart_tx_ring(uart_ring *q);

// Note: The following functions are now defined as static inline in uart.h:
// - get_ring_by_number, get_char, injectc, put_char
// - putch, print, puthx, puth, puth4, hexdump
