#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static inline bool ring_push(volatile uint32_t *w_ptr, volatile uint32_t *r_ptr,
                             uint32_t fifo_size, uint8_t *elems,
                             const void *elem, size_t elem_size, bool overwrite) {
  uint32_t next_w_ptr = (*w_ptr + 1U) % fifo_size;

  if ((next_w_ptr == *r_ptr) && overwrite) {
    *r_ptr = (*r_ptr + 1U) % fifo_size;
  }

  if (next_w_ptr != *r_ptr) {
    (void)memcpy(&elems[*w_ptr * elem_size], elem, elem_size);
    *w_ptr = next_w_ptr;
    return true;
  }

  return false;
}

static inline bool ring_pop(volatile uint32_t *w_ptr, volatile uint32_t *r_ptr,
                            uint32_t fifo_size, uint8_t *elems,
                            void *elem, size_t elem_size) {
  if (*w_ptr != *r_ptr) {
    if (elem != NULL) {
      (void)memcpy(elem, &elems[*r_ptr * elem_size], elem_size);
    }
    *r_ptr = (*r_ptr + 1U) % fifo_size;
    return true;
  }
  return false;
}

static inline uint32_t ring_slots_empty(const volatile uint32_t *w_ptr, const volatile uint32_t *r_ptr,
                                        uint32_t fifo_size) {
  if (*w_ptr >= *r_ptr) {
    return fifo_size - 1U - *w_ptr + *r_ptr;
  } else {
    return *r_ptr - *w_ptr - 1U;
  }
}

static inline void ring_clear(volatile uint32_t *w_ptr, volatile uint32_t *r_ptr) {
  *w_ptr = 0U;
  *r_ptr = 0U;
}

