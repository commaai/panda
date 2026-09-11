#pragma once

#include "board/drivers/register_declarations.h"

// Write only the requested fields. Expected values never come from hardware readback.
void register_set(const tracked_register *reg, uint32_t val, uint32_t mask) {
  ENTER_CRITICAL()
  *reg->address = (*reg->address & ~mask) | (val & mask);
  reg->state->expected = (reg->state->expected & ~mask) | (val & mask);
  reg->state->check_mask |= mask;
  EXIT_CRITICAL()
}

void register_set_bits(const tracked_register *reg, uint32_t val) {
  register_set(reg, val, val);
}

void register_clear_bits(const tracked_register *reg, uint32_t val) {
  register_set(reg, ~val, val);
}

void check_registers(void) {
  for (uint32_t i = 0U; i < (sizeof(tracked_registers) / sizeof(tracked_registers[0])); i++) {
    const tracked_register *reg = tracked_registers[i];
    ENTER_CRITICAL()
    // Unconfigured peripherals may be clock-gated: do not read their registers.
    if (reg->state->check_mask != 0U) {
      uint32_t actual = *reg->address;
      if ((actual & reg->state->check_mask) != (reg->state->expected & reg->state->check_mask)) {
        if (!reg->state->logged_fault) {
          print("Register 0x"); puth((uint32_t) reg->address); print(" divergent! Expected: 0x"); puth(reg->state->expected); print(" Reg: 0x"); puth(actual); print("\n");
          reg->state->logged_fault = true;
        }
        fault_occurred(FAULT_REGISTER_DIVERGENT);
      }
    }
    EXIT_CRITICAL()
  }
}

void init_registers(void) {
  for (uint32_t i = 0U; i < (sizeof(tracked_registers) / sizeof(tracked_registers[0])); i++) {
    tracked_registers[i]->state->expected = 0U;
    tracked_registers[i]->state->check_mask = 0U;
    tracked_registers[i]->state->logged_fault = false;
  }
}
