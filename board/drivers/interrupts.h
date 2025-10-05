#include "interrupts_declarations.h"

void unused_interrupt_handler(void) {
  // Something is wrong if this handler is called!
  print("Unused interrupt handler called!\n");
  fault_occurred(FAULT_UNUSED_INTERRUPT_HANDLED);
}

interrupt interrupts[NUM_INTERRUPTS];

static bool check_interrupt_rate = false;

static uint32_t idle_time = 0U;
static uint32_t busy_time = 0U;
float interrupt_load = 0.0f;

// Track NVIC priority grouping used for encoding priorities
static uint32_t nvic_priority_grouping = IRQ_PRIORITY_GROUPING_DEFAULT;

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

  // Check that the interrupts don't fire too often
  if (check_interrupt_rate && (interrupts[irq_type].call_counter > interrupts[irq_type].max_call_rate)) {
    fault_occurred(interrupts[irq_type].call_rate_fault);
  }

  ENTER_CRITICAL();
  interrupt_depth -= 1U;
  if (interrupt_depth == 0U) {
    uint32_t time = microsecond_timer_get();
    busy_time += get_ts_elapsed(time, last_time);
    last_time = time;
  }
  EXIT_CRITICAL();
}

// Every second
void interrupt_timer_handler(void) {
  if (INTERRUPT_TIMER->SR != 0U) {
    for (uint16_t i = 0U; i < NUM_INTERRUPTS; i++) {
      // Log IRQ call rate faults
      if (check_interrupt_rate && (interrupts[i].call_counter > interrupts[i].max_call_rate)) {
        print("Interrupt 0x"); puth(i); print(" fired too often (0x"); puth(interrupts[i].call_counter); print("/s)!\n");
      }

      // Reset interrupt counters
      interrupts[i].call_rate = interrupts[i].call_counter;
      interrupts[i].call_counter = 0U;
    }

    // Calculate interrupt load
    // The bootstub does not have the FPU enabled, so can't do float operations.
#if !defined(BOOTSTUB)
    interrupt_load = ((busy_time + idle_time) > 0U) ? ((float) (((float) busy_time) / (busy_time + idle_time))) : 0.0f;
#endif
    idle_time = 0U;
    busy_time = 0U;
  }
  INTERRUPT_TIMER->SR = 0;
}

void interrupts_set_priority_grouping(uint32_t grouping) {
  // Set NVIC priority grouping (0..7), 0 -> all preempt priority bits
  nvic_priority_grouping = grouping;
  NVIC_SetPriorityGrouping(nvic_priority_grouping);
}

void interrupts_set_priority(IRQn_Type irq_type, uint8_t preempt_prio, uint8_t sub_prio) {
  if ((uint32_t)irq_type < NUM_INTERRUPTS) {
    interrupts[irq_type].preempt_prio = preempt_prio;
    interrupts[irq_type].sub_prio = sub_prio;
    uint32_t encoded = NVIC_EncodePriority(NVIC_GetPriorityGrouping(), preempt_prio, sub_prio);
    NVIC_SetPriority(irq_type, encoded);
  }
}

void init_interrupts(bool check_rate_limit){
  check_interrupt_rate = check_rate_limit;

  for(uint16_t i=0U; i<NUM_INTERRUPTS; i++){
    interrupts[i].handler = unused_interrupt_handler;
    // Initialize default priorities in the descriptor table
    interrupts[i].preempt_prio = IRQ_DEFAULT_PREEMPT_PRIORITY;
    interrupts[i].sub_prio = IRQ_DEFAULT_SUBPRIORITY;
  }

  // Init interrupt timer for a 1s interval
  interrupt_timer_init();

  // Configure grouping and apply default priorities to all external IRQs
  interrupts_set_priority_grouping(IRQ_PRIORITY_GROUPING_DEFAULT);
  for (uint16_t i = 0U; i < NUM_INTERRUPTS; i++) {
    interrupts_set_priority((IRQn_Type)i, IRQ_DEFAULT_PREEMPT_PRIORITY, IRQ_DEFAULT_SUBPRIORITY);
  }
}
