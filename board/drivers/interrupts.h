#ifndef DRIVERS_INTERRUPTS_H
#define DRIVERS_INTERRUPTS_H

#include "board/drivers/drivers.h"

// interrupt struct and related macros are now defined in drivers.h
// This file only contains the implementation

void unused_interrupt_handler(void) {
  // Something is wrong if this handler is called!
  print("Unused interrupt handler called!\n");
  fault_occurred(FAULT_UNUSED_INTERRUPT_HANDLED);
}

// Global interrupt array - defined here, declared in drivers.h
interrupt interrupts[NUM_INTERRUPTS];

static bool check_interrupt_rate = false;

static uint32_t idle_time = 0U;
static uint32_t busy_time = 0U;
// interrupt_load is declared in drivers.h

void handle_interrupt(IRQn_Type irq_type){
  static uint8_t interrupt_depth = 0U;
  static uint32_t last_time = 0U;
  ENTER_CRITICAL();
  if (interrupt_depth == 0U) {
    uint32_t time = microsecond_timer_get();
    idle_time += get_ts_elapsed(time, last_time);
    last_time = time;
  }
  interrupt_depth += 1U;
  EXIT_CRITICAL();

  interrupts[irq_type].call_counter++;
  interrupts[irq_type].handler();

  EXIT_CRITICAL();
  interrupt_depth -= 1U;
  ENTER_CRITICAL();

  if (interrupt_depth == 0U) {
    last_time = microsecond_timer_get();
    busy_time += get_ts_elapsed(last_time, time);
  }
}

void interrupt_timer_handler(void) {
  static uint32_t call_rate_counters[NUM_INTERRUPTS] = {0};
  uint32_t i;

  for (i = 0U; i < NUM_INTERRUPTS; i++) {
    if (interrupts[i].handler != unused_interrupt_handler) {
      call_rate_counters[i] += interrupts[i].call_counter;
      if (check_interrupt_rate && (call_rate_counters[i] > interrupts[i].max_call_rate)) {
        interrupts[i].call_rate_fault += 1U;
      }
      interrupts[i].call_counter = 0U;
    }
  }
  interrupt_load = ((float)busy_time) / ((float)(busy_time + idle_time));
  busy_time = 0U;
  idle_time = 0U;
}

void init_interrupts(bool check_rate_limit) {
  uint32_t i;
  check_interrupt_rate = check_rate_limit;
  for (i = 0U; i < NUM_INTERRUPTS; i++) {
    interrupts[i].handler = unused_interrupt_handler;
    interrupts[i].call_counter = 0U;
    interrupts[i].call_rate = 0U;
    interrupts[i].call_rate_fault = 0U;
  }
}

#endif
