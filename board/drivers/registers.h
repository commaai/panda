#ifndef REGISTERS_H
#define REGISTERS_H

#include <stdint.h>
#include <stdbool.h>
#include "board/drivers/driver_declarations.h"

typedef struct reg {
  volatile uint32_t *address;
  uint32_t value;
  uint32_t check_mask;
  bool logged_fault;
} reg;

#define CHECK_COLLISION(hash, addr) (((uint32_t) register_map[hash].address != 0U) && (register_map[hash].address != (addr)))

extern reg register_map[REGISTER_MAP_SIZE];

uint16_t hash_addr(uint32_t input);
void register_set(volatile uint32_t *addr, uint32_t val, uint32_t mask);
void register_set_bits(volatile uint32_t *addr, uint32_t val);
void register_clear_bits(volatile uint32_t *addr, uint32_t val);
void check_registers(void);
void init_registers(void);

#endif
