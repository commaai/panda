#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct reg {
  volatile uint32_t *address;
  uint32_t value;
  uint32_t check_mask;
  bool logged_fault;
} reg;

// 10 bit hash with 23 as a prime
#define REGISTER_MAP_SIZE 0x3FFU
#define HASHING_PRIME 23U
#define CHECK_COLLISION(hash, addr) (((uint32_t) register_map[hash].address != 0U) && (register_map[hash].address != (addr)))

// Function declarations
void register_set(volatile uint32_t *addr, uint32_t val, uint32_t mask);
void register_set_bits(volatile uint32_t *addr, uint32_t val);
void register_clear_bits(volatile uint32_t *addr, uint32_t val);
void check_registers(void);
void init_registers(void);

// External declaration for register_map (needed by CHECK_COLLISION macro)
extern reg register_map[REGISTER_MAP_SIZE];
