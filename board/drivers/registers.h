#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "board/critical.h"

#define FAULT_STATUS_NONE 0U
#define FAULT_STATUS_TEMPORARY 1U
#define FAULT_STATUS_PERMANENT 2U

// Fault types, matches cereal.log.PandaState.FaultType
#define FAULT_RELAY_MALFUNCTION             (1UL << 0)
#define FAULT_UNUSED_INTERRUPT_HANDLED      (1UL << 1)
#define FAULT_INTERRUPT_RATE_CAN_1          (1UL << 2)
#define FAULT_INTERRUPT_RATE_CAN_2          (1UL << 3)
#define FAULT_INTERRUPT_RATE_CAN_3          (1UL << 4)
#define FAULT_INTERRUPT_RATE_TACH           (1UL << 5)
#define FAULT_INTERRUPT_RATE_INTERRUPTS     (1UL << 7)
#define FAULT_INTERRUPT_RATE_SPI_DMA        (1UL << 8)
#define FAULT_INTERRUPT_RATE_USB            (1UL << 15)
#define FAULT_REGISTER_DIVERGENT            (1UL << 18)
#define FAULT_INTERRUPT_RATE_CLOCK_SOURCE   (1UL << 20)
#define FAULT_INTERRUPT_RATE_TICK           (1UL << 21)
#define FAULT_INTERRUPT_RATE_EXTI           (1UL << 22)
#define FAULT_INTERRUPT_RATE_SPI            (1UL << 23)
#define FAULT_INTERRUPT_RATE_UART_7         (1UL << 24)
#define FAULT_SIREN_MALFUNCTION             (1UL << 25)
#define FAULT_HEARTBEAT_LOOP_WATCHDOG       (1UL << 26)
#define FAULT_INTERRUPT_RATE_SOUND_DMA      (1UL << 27)

// 10 bit hash with 23 as a prime
#define REGISTER_MAP_SIZE 0x3FFU
#define HASHING_PRIME 23U
#define CHECK_COLLISION(hash, addr) (((uint32_t) register_map[hash].address != 0U) && (register_map[hash].address != (addr)))

typedef struct reg {
  volatile uint32_t *address;
  uint32_t value;
  uint32_t check_mask;
  bool logged_fault;
} reg;

// Do not put bits in the check mask that get changed by the hardware
void register_set(volatile uint32_t *addr, uint32_t val, uint32_t mask);
// Set individual bits. Also add them to the check_mask.
// Do not use this to change bits that get reset by the hardware
void register_set_bits(volatile uint32_t *addr, uint32_t val);
// Clear individual bits. Also add them to the check_mask.
// Do not use this to clear bits that get set by the hardware
void register_clear_bits(volatile uint32_t *addr, uint32_t val);
// To be called periodically
void check_registers(void);
void init_registers(void);