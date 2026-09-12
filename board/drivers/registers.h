#pragma once

#include "board/drivers/drivers.h"

typedef struct reg {
  uint32_t value;
  uint32_t check_mask;
  bool logged_fault;
} reg;

#define REGISTER_COUNT (sizeof(register_addresses) / sizeof(register_addresses[0]))
static reg register_map[REGISTER_COUNT];

// Do not put bits in the check mask that get changed by the hardware
void register_set(volatile uint32_t *addr, uint32_t val, uint32_t mask){
  ENTER_CRITICAL()
  uint32_t i = 0U;
  while ((i < REGISTER_COUNT) && (register_addresses[i] != addr)) { i++; }
  if (i < REGISTER_COUNT) {
    // Set bits in register that are also in the mask
    (*addr) = ((*addr) & (~mask)) | (val & mask);
    register_map[i].value = (register_map[i].value & (~mask)) | (val & mask);
    register_map[i].check_mask |= mask;
  } else {
    assert_fatal(false, "Register missing from monitoring inventory\n");
  }
  EXIT_CRITICAL()
}

// Set individual bits. Also add them to the check_mask.
// Do not use this to change bits that get reset by the hardware
void register_set_bits(volatile uint32_t *addr, uint32_t val) {
  register_set(addr, val, val);
}

// Clear individual bits. Also add them to the check_mask.
// Do not use this to clear bits that get set by the hardware
void register_clear_bits(volatile uint32_t *addr, uint32_t val) {
  register_set(addr, (~val), val);
}

// To be called periodically
void check_registers(void){
  for(uint16_t i=0U; i<REGISTER_COUNT; i++){
    if(register_map[i].check_mask != 0U){
      ENTER_CRITICAL()
      if((*(register_addresses[i]) & register_map[i].check_mask) != (register_map[i].value & register_map[i].check_mask)){
        if(!register_map[i].logged_fault){
          print("Register 0x"); puth((uint32_t) register_addresses[i]); print(" divergent! Map: 0x"); puth(register_map[i].value); print(" Reg: 0x"); puth(*(register_addresses[i])); print("\n");
          register_map[i].logged_fault = true;
        }
        fault_occurred(FAULT_REGISTER_DIVERGENT);
      }
      EXIT_CRITICAL()
    }
  }
}

void init_registers(void) {
  for(uint16_t i=0U; i<REGISTER_COUNT; i++){
    register_map[i].value = 0U;
    register_map[i].check_mask = 0U;
    register_map[i].logged_fault = false;
  }
}
