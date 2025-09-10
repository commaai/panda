#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

static inline bool rb_pop(volatile uint32_t *w_ptr, volatile uint32_t *r_ptr,
                          uint32_t fifo_size, void *elems, uint32_t elem_size,
                          void *elem) {
  bool ret = false;

  ENTER_CRITICAL();
  if (*w_ptr != *r_ptr) {
    if (elem != NULL) {
      (void)memcpy(elem, ((uint8_t *)elems) + (*r_ptr * elem_size), elem_size);
    }
    *r_ptr = (*r_ptr + 1U) % fifo_size;
    ret = true;
  }
  EXIT_CRITICAL();

  return ret;
}

static inline bool rb_push(volatile uint32_t *w_ptr, volatile uint32_t *r_ptr,
                           uint32_t fifo_size, void *elems, uint32_t elem_size,
                           const void *elem, bool overwrite) {
  bool ret = false;
  uint32_t next_w_ptr = (*w_ptr + 1U) % fifo_size;

  ENTER_CRITICAL();
  if ((next_w_ptr == *r_ptr) && overwrite) {
    *r_ptr = (*r_ptr + 1U) % fifo_size;
  }

  if (next_w_ptr != *r_ptr) {
    (void)memcpy(((uint8_t *)elems) + (*w_ptr * elem_size), elem, elem_size);
    *w_ptr = next_w_ptr;
    ret = true;
  }
  EXIT_CRITICAL();

  return ret;
}

static inline uint32_t rb_slots_empty(const volatile uint32_t *w_ptr,
                                      const volatile uint32_t *r_ptr,
                                      uint32_t fifo_size) {
  uint32_t ret;

  ENTER_CRITICAL();
  if (*w_ptr >= *r_ptr) {
    ret = fifo_size - 1U - *w_ptr + *r_ptr;
  } else {
    ret = *r_ptr - *w_ptr - 1U;
  }
  EXIT_CRITICAL();

  return ret;
}

static inline void rb_clear(volatile uint32_t *w_ptr, volatile uint32_t *r_ptr) {
  ENTER_CRITICAL();
  *w_ptr = 0;
  *r_ptr = 0;
  EXIT_CRITICAL();
}

