#if defined(STM32H7)
#include "../stm32h7/inc/stm32h725xx.h"
#else
#include "../stm32f4/inc/stm32f413xx.h"
#endif

#include "uart.h"

#include <stddef.h>
#define UNUSED(x) ((void)(x))

#define ENTER_CRITICAL() __disable_irq()
#define EXIT_CRITICAL()  __enable_irq()


#define UART_BUFFER(x, size_rx, size_tx, uart_ptr, callback_ptr, overwrite_mode) \
  static uint8_t elems_rx_##x[size_rx]; \
  static uint8_t elems_tx_##x[size_tx]; \
  uart_ring uart_ring_##x = { \
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

// debug = USART2
UART_BUFFER(debug, FIFO_SIZE_INT, FIFO_SIZE_INT, USART2, debug_ring_callback, true);

// SOM debug = UART7
#ifdef STM32H7
  UART_BUFFER(som_debug, FIFO_SIZE_INT, FIFO_SIZE_INT, UART7, NULL, true);
#else
  UART_BUFFER(som_debug, 1U, 1U, NULL, NULL, true);
#endif

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
  bool ret = false;
  uint16_t next_w_ptr;

  ENTER_CRITICAL();
  next_w_ptr = (q->w_ptr_rx + 1U) % q->rx_fifo_size;

  if ((next_w_ptr == q->r_ptr_rx) && q->overwrite) {
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

#if defined(STM32H7)
static void uart_tx_ring(uart_ring *q) {
  if ((q->uart->ISR & USART_ISR_TXE_TXFNF) != 0U) {
    if (q->w_ptr_tx != q->r_ptr_tx) {
      q->uart->TDR = q->elems_tx[q->r_ptr_tx];
      q->r_ptr_tx = (q->r_ptr_tx + 1U) % q->tx_fifo_size;
    }
  }
}
#else
static void uart_tx_ring(uart_ring *q) {
  if ((q->uart->SR & USART_SR_TXE) != 0U) {
    if (q->w_ptr_tx != q->r_ptr_tx) {
      q->uart->DR = q->elems_tx[q->r_ptr_tx];
      q->r_ptr_tx = (q->r_ptr_tx + 1U) % q->tx_fifo_size;
    }
  }
}
#endif

bool put_char(uart_ring *q, char elem) {
  bool ret = false;
  uint16_t next_w_ptr;

  ENTER_CRITICAL();
  next_w_ptr = (q->w_ptr_tx + 1U) % q->tx_fifo_size;

  if ((next_w_ptr == q->r_ptr_tx) && q->overwrite) {
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


void putch(const char a) {
  (void)injectc(&uart_ring_debug, a);
}

void print(const char *a) {
  for (const char *in = a; *in; in++) {
    if (*in == '\n') putch('\r');
    putch(*in);
  }
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

void clear_uart_buff(uart_ring *q) {
  ENTER_CRITICAL();
  q->w_ptr_tx = q->r_ptr_tx = 0;
  q->w_ptr_rx = q->r_ptr_rx = 0;
  EXIT_CRITICAL();
}
