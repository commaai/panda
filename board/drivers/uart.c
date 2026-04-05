#include "board/config.h"
#include "board/utils.h"
#include "board/drivers/uart.h"

// Define the rings using the macro from the header
UART_BUFFER(debug, FIFO_SIZE_INT, FIFO_SIZE_INT, USART2, debug_ring_callback, true)
UART_BUFFER(som_debug, FIFO_SIZE_INT, FIFO_SIZE_INT, UART7, NULL, true)

uart_ring *get_ring_by_number(int a) {
  uart_ring *ring = NULL;
  switch(a) {
    case 0:
      ring = &uart_ring_debug;
      break;
    case 4:
      ring = &uart_ring_som_debug;
      break;
    default:
      ring = NULL;
      break;
  }
  return ring;
}

bool get_char(uart_ring *q, char *elem) {
  bool ret = false;

  ENTER_CRITICAL();
  if (q->w_ptr_rx != q->r_ptr_rx) {
    if (elem != NULL) *elem = q->elems_rx[q->r_ptr_rx];
    q->r_ptr_rx = (q->r_ptr_rx + 1U) % q->rx_fifo_size;
    ret = true;
  }
  EXIT_CRITICAL();

  return ret;
}

bool injectc(uart_ring *q, char elem) {
  int ret = false;
  uint16_t next_w_ptr;

  ENTER_CRITICAL();
  next_w_ptr = (q->w_ptr_rx + 1U) % q->rx_fifo_size;

  if ((next_w_ptr == q->r_ptr_rx) && q->overwrite) {
    // overwrite mode: drop oldest byte
    q->r_ptr_rx = (q->r_ptr_rx + 1U) % q->rx_fifo_size;
  }

  if (next_w_ptr != q->r_ptr_rx) {
    q->elems_rx[q->w_ptr_rx] = elem;
    q->w_ptr_rx = next_w_ptr;
    ret = true;
  }
  EXIT_CRITICAL();

  return ret;
}

bool put_char(uart_ring *q, char elem) {
  bool ret = false;
  uint16_t next_w_ptr;

  ENTER_CRITICAL();
  next_w_ptr = (q->w_ptr_tx + 1U) % q->tx_fifo_size;

  if ((next_w_ptr == q->r_ptr_tx) && q->overwrite) {
    // overwrite mode: drop oldest byte
    q->r_ptr_tx = (q->r_ptr_tx + 1U) % q->tx_fifo_size;
  }

  if (next_w_ptr != q->r_ptr_tx) {
    q->elems_tx[q->w_ptr_tx] = elem;
    q->w_ptr_tx = next_w_ptr;
    ret = true;
  }
  EXIT_CRITICAL();

#ifndef BOOTSTUB
  uart_tx_ring(q);
#endif

  return ret;
}

void putch(const char a) {
  (void)injectc(&uart_ring_debug, a);
}

#ifndef BOOTSTUB
void print(const char *a) {
  for (const char *in = a; *in; in++) {
    if (*in == '\n') putch('\r');
    putch(*in);
  }
}
#endif

#ifndef BOOTSTUB
void puthx(uint32_t i, uint8_t len) {
  const char c[] = "0123456789abcdef";
  for (int pos = ((int)len * 4) - 4; pos > -4; pos -= 4) {
    putch(c[(i >> (unsigned int)(pos)) & 0xFU]);
  }
}

void puth(unsigned int i) {
  puthx(i, 8U);
}

#if defined(DEBUG_SPI) || defined(DEBUG)
void puth4(unsigned int i) {
  puthx(i, 4U);
}
#endif

#if defined(DEBUG_SPI) || defined(DEBUG_USB) || defined(DEBUG_COMMS)
void hexdump(const void *a, int l) {
  if (a != NULL) {
    for (int i=0; i < l; i++) {
      if ((i != 0) && ((i & 0xf) == 0)) print("\n");
      puthx(((const unsigned char*)a)[i], 2U);
      print(" ");
    }
  }
  print("\n");
}
#endif
#endif
