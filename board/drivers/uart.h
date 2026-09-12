#pragma once

#include "board/drivers/drivers.h"

// ***************************** Definitions *****************************

#define UART_BUFFER(x, size_rx, size_tx, uart_ptr, overwrite_mode) \
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
    .overwrite = (overwrite_mode) \
  };

// ******************************** UART buffers ********************************

// SOM debug = UART7
UART_BUFFER(som_debug, FIFO_SIZE_INT, FIFO_SIZE_INT, UART7, true)

uart_ring *get_ring_by_number(int a) {
  uart_ring *ring = NULL;
  switch(a) {
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
