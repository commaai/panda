#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "board/config.h"

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
void uart_tx_ring(uart_ring *q);
uart_ring *get_ring_by_number(int a);
// ************************* Low-level buffer functions *************************
bool get_char(uart_ring *q, char *elem);
bool injectc(uart_ring *q, char elem);
bool put_char(uart_ring *q, char elem);
void clear_uart_buff(uart_ring *q);
// ************************ High-level debug functions **********************
void putch(const char a);
void print(const char *a);
void puthx(uint32_t i, uint8_t len);
void puth(unsigned int i);
#if defined(DEBUG_SPI) || defined(BOOTSTUB) || defined(DEBUG)
void puth4(unsigned int i);
#endif
#if defined(DEBUG_SPI) || defined(DEBUG_USB) || defined(DEBUG_COMMS)
void hexdump(const void *a, int l);
#endif


// ***************************** Definitions *****************************

#define UART_BUFFER(x, size_rx, size_tx, uart_ptr, callback_ptr, overwrite_mode) \
  static uint8_t elems_rx_##x[size_rx]; \
  static uint8_t elems_tx_##x[size_tx]; \
  extern uart_ring uart_ring_##x; \
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

// ******************************** UART buffers ********************************

extern uart_ring uart_ring_som_debug;

uart_ring *get_ring_by_number(int a);

// ************************* Low-level buffer functions *************************
bool get_char(uart_ring *q, char *elem);
bool injectc(uart_ring *q, char elem);
bool put_char(uart_ring *q, char elem);

// ************************ High-level debug functions **********************
void putch(const char a);
void print(const char *a);
void puthx(uint32_t i, uint8_t len);
void puth(unsigned int i);