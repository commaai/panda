#pragma once
#include <stdbool.h>
#include "faults_panda.h"
#include "config.h"
#include "platform_definitions.h"

typedef struct interrupt {
  IRQn_Type irq_type;
  void (*handler)(void);
  uint32_t call_counter;
  uint32_t call_rate;
  uint32_t max_call_rate;   // Call rate is defined as the amount of calls each second
  uint32_t call_rate_fault;
} interrupt;

void interrupt_timer_init(void);
void unused_interrupt_handler(void);

extern interrupt interrupts[NUM_INTERRUPTS];

#define REGISTER_INTERRUPT(irq_num, func_ptr, call_rate_max, rate_fault) \
  interrupts[irq_num].irq_type = (irq_num); \
  interrupts[irq_num].handler = (func_ptr);  \
  interrupts[irq_num].call_counter = 0U;   \
  interrupts[irq_num].call_rate = 0U;   \
  interrupts[irq_num].max_call_rate = (call_rate_max); \
  interrupts[irq_num].call_rate_fault = (rate_fault);

extern float interrupt_load;

void handle_interrupt(IRQn_Type irq_type);
void enable_interrupt_timer(void);
// Every second
void interrupt_timer_handler(void);
void init_interrupts(bool check_rate_limit);
