#ifndef DRIVERS_REGISTERS_H
#define DRIVERS_REGISTERS_H

#include <stdint.h>
#include <stdbool.h>

void register_set(volatile uint32_t *addr, uint32_t val, uint32_t mask);
void register_set_bits(volatile uint32_t *addr, uint32_t val);
void register_clear_bits(volatile uint32_t *addr, uint32_t val);
void check_registers(void);
void init_registers(void);

#endif
