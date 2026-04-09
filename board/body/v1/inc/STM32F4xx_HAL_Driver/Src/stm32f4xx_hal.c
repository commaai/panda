#include "stm32f4xx_hal.h"

#ifdef HAL_MODULE_ENABLED

__IO uint32_t uwTick;
uint32_t uwTickPrio = (1UL << __NVIC_PRIO_BITS);
HAL_TickFreqTypeDef uwTickFreq = HAL_TICK_FREQ_DEFAULT;

HAL_StatusTypeDef HAL_Init(void) {
#if (INSTRUCTION_CACHE_ENABLE != 0U)
  __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
#endif
#if (DATA_CACHE_ENABLE != 0U)
  __HAL_FLASH_DATA_CACHE_ENABLE();
#endif
#if (PREFETCH_ENABLE != 0U)
  __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
#endif

  HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
  HAL_InitTick(TICK_INT_PRIORITY);
  HAL_MspInit();
  return HAL_OK;
}

__weak void HAL_MspInit(void) {
}

__weak HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority) {
  if (HAL_SYSTICK_Config(SystemCoreClock / (1000U / uwTickFreq)) > 0U) {
    return HAL_ERROR;
  }

  if (TickPriority < (1UL << __NVIC_PRIO_BITS)) {
    HAL_NVIC_SetPriority(SysTick_IRQn, TickPriority, 0U);
    uwTickPrio = TickPriority;
  } else {
    return HAL_ERROR;
  }

  return HAL_OK;
}

__weak void HAL_IncTick(void) {
  uwTick += uwTickFreq;
}

__weak uint32_t HAL_GetTick(void) {
  return uwTick;
}

__weak void HAL_Delay(uint32_t Delay) {
  uint32_t tickstart = HAL_GetTick();
  uint32_t wait = Delay;

  if (wait < HAL_MAX_DELAY) {
    wait += (uint32_t)uwTickFreq;
  }

  while ((HAL_GetTick() - tickstart) < wait) {
  }
}

#endif
