#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint32_t expected;
  uint32_t check_mask;
  bool logged_fault;
} tracked_register_state;

// Descriptors live in flash so early initialization can use them before .data is copied.
typedef struct {
  volatile uint32_t *address;
  tracked_register_state *state;
} tracked_register;

typedef struct tracked_gpio tracked_gpio;
typedef struct tracked_timer tracked_timer;
typedef struct tracked_i2c tracked_i2c;

// Only software-controlled, readable bits belong in the mask.
void register_set(const tracked_register *reg, uint32_t val, uint32_t mask);
void register_set_bits(const tracked_register *reg, uint32_t val);
void register_clear_bits(const tracked_register *reg, uint32_t val);
void check_registers(void);
void init_registers(void);
