#pragma once
#include <stdbool.h>
#include <stdint.h>

// Use existing critical-section macros if present; fall back to no-ops for tests.
#ifndef ENTER_CRITICAL
#define ENTER_CRITICAL() do {} while (0)
#endif
#ifndef EXIT_CRITICAL
#define EXIT_CRITICAL() do {} while (0)
#endif

// Full push/pop helpers operating on element buffers.
// These are small static inline functions for zero call overhead.

static inline bool rb_pop_u16(void *elems, uint16_t elem_size,
                              volatile uint16_t *w_ptr, volatile uint16_t *r_ptr,
                              uint16_t size, void *out) {
  bool ret = false;
  ENTER_CRITICAL();
  if (*w_ptr != *r_ptr) {
    if (out != NULL) {
      (void)memcpy(out, (uint8_t *)elems + ((uint32_t)(*r_ptr) * elem_size), elem_size);
    }
    *r_ptr = (uint16_t)((*r_ptr + 1U) % size);
    ret = true;
  }
  EXIT_CRITICAL();
  return ret;
}

static inline bool rb_push_u16(void *elems, uint16_t elem_size,
                               volatile uint16_t *w_ptr, volatile uint16_t *r_ptr,
                               uint16_t size, const void *in, bool overwrite) {
  bool ret = false;
  ENTER_CRITICAL();
  uint16_t next_w = (uint16_t)((*w_ptr + 1U) % size);
  if ((next_w == *r_ptr) && overwrite) {
    *r_ptr = (uint16_t)((*r_ptr + 1U) % size);
  }
  if (next_w != *r_ptr) {
    if (in != NULL) {
      (void)memcpy((uint8_t *)elems + ((uint32_t)(*w_ptr) * elem_size), in, elem_size);
    }
    *w_ptr = next_w;
    ret = true;
  }
  EXIT_CRITICAL();
  return ret;
}

static inline bool rb_pop_u32(void *elems, uint32_t elem_size,
                              volatile uint32_t *w_ptr, volatile uint32_t *r_ptr,
                              uint32_t size, void *out) {
  bool ret = false;
  ENTER_CRITICAL();
  if (*w_ptr != *r_ptr) {
    if (out != NULL) {
      (void)memcpy(out, (uint8_t *)elems + ((*r_ptr) * elem_size), elem_size);
    }
    uint32_t next_r = *r_ptr + 1U;
    *r_ptr = (next_r == size) ? 0U : next_r;
    ret = true;
  }
  EXIT_CRITICAL();
  return ret;
}

static inline bool rb_push_u32(void *elems, uint32_t elem_size,
                               volatile uint32_t *w_ptr, volatile uint32_t *r_ptr,
                               uint32_t size, const void *in, bool overwrite) {
  bool ret = false;
  ENTER_CRITICAL();
  uint32_t next_w = *w_ptr + 1U;
  next_w = (next_w == size) ? 0U : next_w;
  if ((next_w == *r_ptr) && overwrite) {
    uint32_t next_r = *r_ptr + 1U;
    *r_ptr = (next_r == size) ? 0U : next_r;
  }
  if (next_w != *r_ptr) {
    if (in != NULL) {
      (void)memcpy((uint8_t *)elems + ((*w_ptr) * elem_size), in, elem_size);
    }
    *w_ptr = next_w;
    ret = true;
  }
  EXIT_CRITICAL();
  return ret;
}
