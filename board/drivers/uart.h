#pragma once

#include <stdint.h>
#include <stdbool.h>

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

extern uart_ring uart_ring_debug;
extern uart_ring uart_ring_som_debug;

void debug_ring_callback(uart_ring *ring);
uart_ring *get_ring_by_number(int a);

bool get_char(uart_ring *q, char *elem);
bool injectc(uart_ring *q, char elem);
bool put_char(uart_ring *q, char elem);
void clear_uart_buff(uart_ring *q);

void putch(const char a);
void print(const char *a);
void puthx(uint32_t i, uint8_t len);
void puth(unsigned int i);

#if defined(DEBUG_SPI) || defined(BOOTSTUB) || defined(DEBUG)
static inline void puth4(unsigned int i) { puthx(i, 4U); }
#endif

#if defined(DEBUG_SPI) || defined(DEBUG_USB) || defined(DEBUG_COMMS)
static inline void hexdump(const void *a, int l) {
  if (a != NULL) {
    for (int i = 0; i < l; i++) {
      if ((i != 0) && ((i & 0xf) == 0)) print("\n");
      puthx(((const unsigned char*)a)[i], 2U);
      print(" ");
    }
  }
  print("\n");
}
#endif
