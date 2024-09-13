#include "uart.h"
#include "critical.h"

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

// ************************* Low-level buffer functions *************************
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

  uart_tx_ring(q);

  return ret;
}

void clear_uart_buff(uart_ring *q) {
  ENTER_CRITICAL();
  q->w_ptr_tx = 0;
  q->r_ptr_tx = 0;
  q->w_ptr_rx = 0;
  q->r_ptr_rx = 0;
  EXIT_CRITICAL();
}

// ************************ High-level debug functions **********************
void putch(const char a) {
  // misra-c2012-17.7: serial debug function, ok to ignore output
  (void)injectc(&uart_ring_debug, a);
}

void print(const char *a) {
  for (const char *in = a; *in; in++) {
    if (*in == '\n') putch('\r');
    putch(*in);
  }
}

void putui(uint32_t i) {
  uint32_t i_copy = i;
  char str[11];
  uint8_t idx = 10;
  str[idx] = '\0';
  idx--;
  do {
    str[idx] = (i_copy % 10U) + 0x30U;
    idx--;
    i_copy /= 10;
  } while (i_copy != 0U);
  print(&str[idx + 1U]);
}

void puthx(uint32_t i, uint8_t len) {
  const char c[] = "0123456789abcdef";
  for (int pos = ((int)len * 4) - 4; pos > -4; pos -= 4) {
    putch(c[(i >> (unsigned int)(pos)) & 0xFU]);
  }
}

void puth(unsigned int i) {
  puthx(i, 8U);
}

void puth2(unsigned int i) {
  puthx(i, 2U);
}

void puth4(unsigned int i) {
  puthx(i, 4U);
}

void hexdump(const void *a, int l) {
  if (a != NULL) {
    for (int i=0; i < l; i++) {
      if ((i != 0) && ((i & 0xf) == 0)) print("\n");
      puth2(((const unsigned char*)a)[i]);
      print(" ");
    }
  }
  print("\n");
}
