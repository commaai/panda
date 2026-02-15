#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "board/config.h"

// ======================= CRITICAL SECTION =======================

extern uint8_t global_critical_depth;

#ifndef ENTER_CRITICAL
#define ENTER_CRITICAL()                                      \
  __disable_irq();                                            \
  global_critical_depth += 1U;
#endif

#ifndef EXIT_CRITICAL
#define EXIT_CRITICAL()                                       \
  global_critical_depth -= 1U;                                \
  if ((global_critical_depth == 0U) && interrupts_enabled) {  \
    __enable_irq();                                           \
  }
#endif

extern volatile bool interrupts_enabled;

void enable_interrupts(void);
void disable_interrupts(void);

// ======================= FAULTS =======================

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

// Permanent faults
#define PERMANENT_FAULTS 0U

extern uint8_t fault_status;
extern uint32_t faults;

void fault_occurred(uint32_t fault);
void fault_recovered(uint32_t fault);

// ======================= FLASHER =======================

// from the linker script
#define APP_START_ADDRESS 0x8020000U

// flasher state variables
extern uint32_t *prog_ptr;
extern bool unlocked;

void soft_flasher_start(void);

// ======================= PROVISION =======================

// this is where we manage the dongle ID assigned during our
// manufacturing. aside from this, there's a UID for the MCU

#define PROVISION_CHUNK_LEN 0x20

void get_provision_chunk(uint8_t *resp);

// ======================= LIBC =======================

#define UNALIGNED(X, Y) \
  (((uint32_t)(X) & (sizeof(uint32_t) - 1U)) | ((uint32_t)(Y) & (sizeof(uint32_t) - 1U)))

__attribute__((aligned(32), noinline)) void delay(uint32_t a);
void assert_fatal(bool condition, const char *msg);
// cppcheck-suppress misra-c2012-21.2
void *memset(void *str, int c, unsigned int n);
// cppcheck-suppress misra-c2012-21.2
void *memcpy(void *dest, const void *src, unsigned int len);
// cppcheck-suppress misra-c2012-21.2
int memcmp(const void * ptr1, const void * ptr2, unsigned int num);

// ======================= PRINT =======================

void putch(const char a);
void print(const char *a);
void puthx(uint32_t i, uint8_t len);
void puth(unsigned int i);
#if defined(DEBUG_SPI) || defined(BOOTSTUB) || defined(DEBUG)
void puth4(unsigned int i);
#endif
#if defined(DEBUG_SPI) || defined(DEBUG_USB) || defined(DEBUG_COMMS)
void hexdump(const void *a, int l);
#endif
