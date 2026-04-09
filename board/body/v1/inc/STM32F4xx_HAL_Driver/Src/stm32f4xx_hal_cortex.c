#include "stm32f4xx_hal.h"

#ifdef HAL_CORTEX_MODULE_ENABLED

void HAL_NVIC_SetPriorityGrouping(uint32_t PriorityGroup) {
  assert_param(IS_NVIC_PRIORITY_GROUP(PriorityGroup));
  NVIC_SetPriorityGrouping(PriorityGroup);
}

void HAL_NVIC_SetPriority(IRQn_Type IRQn, uint32_t PreemptPriority, uint32_t SubPriority) {
  uint32_t prioritygroup = 0x00U;

  assert_param(IS_NVIC_SUB_PRIORITY(SubPriority));
  assert_param(IS_NVIC_PREEMPTION_PRIORITY(PreemptPriority));

  prioritygroup = NVIC_GetPriorityGrouping();
  NVIC_SetPriority(IRQn, NVIC_EncodePriority(prioritygroup, PreemptPriority, SubPriority));
}

void HAL_NVIC_EnableIRQ(IRQn_Type IRQn) {
  assert_param(IS_NVIC_DEVICE_IRQ(IRQn));
  NVIC_EnableIRQ(IRQn);
}

uint32_t HAL_SYSTICK_Config(uint32_t TicksNumb) {
  return SysTick_Config(TicksNumb);
}

void HAL_SYSTICK_CLKSourceConfig(uint32_t CLKSource) {
  assert_param(IS_SYSTICK_CLK_SOURCE(CLKSource));
  if (CLKSource == SYSTICK_CLKSOURCE_HCLK) {
    SysTick->CTRL |= SYSTICK_CLKSOURCE_HCLK;
  } else {
    SysTick->CTRL &= ~SYSTICK_CLKSOURCE_HCLK;
  }
}

#endif
