#pragma once

// Interrupt descriptor and (now) priority metadata
typedef struct interrupt {
  IRQn_Type irq_type;
  void (*handler)(void);
  uint32_t call_counter;
  uint32_t call_rate;
  uint32_t max_call_rate;   // Call rate is defined as the amount of calls each second
  uint32_t call_rate_fault;
  // Priority configuration (preempt priority and subpriority encoded per grouping)
  // Lower numbers are higher priority on ARM NVIC.
  uint8_t preempt_prio;     // 0..(1<<__NVIC_PRIO_BITS)-1
  uint8_t sub_prio;         // typically 0 when using full preempt priority
} interrupt;

void interrupt_timer_init(void);
uint32_t microsecond_timer_get(void);
void unused_interrupt_handler(void);

extern interrupt interrupts[NUM_INTERRUPTS];

// Default priority configuration (fixed, not overridable)
//  - Grouping 0: all available bits used for preempt priority
//  - Baseline preempt priority: 2 (SPI/DMA use 0 to preempt)
//  - Subpriority: 0 (unused with grouping 0)
#define IRQ_PRIORITY_GROUPING_DEFAULT (0U)
#define IRQ_DEFAULT_PREEMPT_PRIORITY  (2U)
#define IRQ_DEFAULT_SUBPRIORITY       (0U)

// Apply defaults on registration and set NVIC priority accordingly.
#define REGISTER_INTERRUPT(irq_num, func_ptr, call_rate_max, rate_fault) \
  interrupts[irq_num].irq_type = (irq_num); \
  interrupts[irq_num].handler = (func_ptr);  \
  interrupts[irq_num].call_counter = 0U;   \
  interrupts[irq_num].call_rate = 0U;   \
  interrupts[irq_num].max_call_rate = (call_rate_max); \
  interrupts[irq_num].call_rate_fault = (rate_fault); \
  interrupts[irq_num].preempt_prio = IRQ_DEFAULT_PREEMPT_PRIORITY; \
  interrupts[irq_num].sub_prio = IRQ_DEFAULT_SUBPRIORITY; \
  interrupts_set_priority((irq_num), IRQ_DEFAULT_PREEMPT_PRIORITY, IRQ_DEFAULT_SUBPRIORITY);

extern float interrupt_load;

void handle_interrupt(IRQn_Type irq_type);
// Every second
void interrupt_timer_handler(void);
void init_interrupts(bool check_rate_limit);

// Priority configuration helpers
void interrupts_set_priority(IRQn_Type irq_type, uint8_t preempt_prio, uint8_t sub_prio);
void interrupts_set_priority_grouping(uint32_t grouping);
