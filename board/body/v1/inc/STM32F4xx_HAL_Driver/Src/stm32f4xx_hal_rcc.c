#include "stm32f4xx_hal.h"

#ifdef HAL_RCC_MODULE_ENABLED

HAL_StatusTypeDef HAL_RCC_ClockConfig(RCC_ClkInitTypeDef *RCC_ClkInitStruct, uint32_t FLatency) {
  uint32_t tickstart;

  if (RCC_ClkInitStruct == NULL) {
    return HAL_ERROR;
  }

  assert_param(IS_RCC_CLOCKTYPE(RCC_ClkInitStruct->ClockType));
  assert_param(IS_FLASH_LATENCY(FLatency));

  if (FLatency > __HAL_FLASH_GET_LATENCY()) {
    __HAL_FLASH_SET_LATENCY(FLatency);
    if (__HAL_FLASH_GET_LATENCY() != FLatency) {
      return HAL_ERROR;
    }
  }

  if (((RCC_ClkInitStruct->ClockType) & RCC_CLOCKTYPE_HCLK) == RCC_CLOCKTYPE_HCLK) {
    if (((RCC_ClkInitStruct->ClockType) & RCC_CLOCKTYPE_PCLK1) == RCC_CLOCKTYPE_PCLK1) {
      MODIFY_REG(RCC->CFGR, RCC_CFGR_PPRE1, RCC_HCLK_DIV16);
    }

    if (((RCC_ClkInitStruct->ClockType) & RCC_CLOCKTYPE_PCLK2) == RCC_CLOCKTYPE_PCLK2) {
      MODIFY_REG(RCC->CFGR, RCC_CFGR_PPRE2, (RCC_HCLK_DIV16 << 3));
    }

    assert_param(IS_RCC_HCLK(RCC_ClkInitStruct->AHBCLKDivider));
    MODIFY_REG(RCC->CFGR, RCC_CFGR_HPRE, RCC_ClkInitStruct->AHBCLKDivider);
  }

  if (((RCC_ClkInitStruct->ClockType) & RCC_CLOCKTYPE_SYSCLK) == RCC_CLOCKTYPE_SYSCLK) {
    assert_param(IS_RCC_SYSCLKSOURCE(RCC_ClkInitStruct->SYSCLKSource));

    if (RCC_ClkInitStruct->SYSCLKSource == RCC_SYSCLKSOURCE_HSE) {
      if (__HAL_RCC_GET_FLAG(RCC_FLAG_HSERDY) == RESET) {
        return HAL_ERROR;
      }
    } else if ((RCC_ClkInitStruct->SYSCLKSource == RCC_SYSCLKSOURCE_PLLCLK) ||
               (RCC_ClkInitStruct->SYSCLKSource == RCC_SYSCLKSOURCE_PLLRCLK)) {
      if (__HAL_RCC_GET_FLAG(RCC_FLAG_PLLRDY) == RESET) {
        return HAL_ERROR;
      }
    } else {
      if (__HAL_RCC_GET_FLAG(RCC_FLAG_HSIRDY) == RESET) {
        return HAL_ERROR;
      }
    }

    __HAL_RCC_SYSCLK_CONFIG(RCC_ClkInitStruct->SYSCLKSource);

    tickstart = HAL_GetTick();
    while (__HAL_RCC_GET_SYSCLK_SOURCE() != (RCC_ClkInitStruct->SYSCLKSource << RCC_CFGR_SWS_Pos)) {
      if ((HAL_GetTick() - tickstart) > CLOCKSWITCH_TIMEOUT_VALUE) {
        return HAL_TIMEOUT;
      }
    }
  }

  if (FLatency < __HAL_FLASH_GET_LATENCY()) {
    __HAL_FLASH_SET_LATENCY(FLatency);
    if (__HAL_FLASH_GET_LATENCY() != FLatency) {
      return HAL_ERROR;
    }
  }

  if (((RCC_ClkInitStruct->ClockType) & RCC_CLOCKTYPE_PCLK1) == RCC_CLOCKTYPE_PCLK1) {
    assert_param(IS_RCC_PCLK(RCC_ClkInitStruct->APB1CLKDivider));
    MODIFY_REG(RCC->CFGR, RCC_CFGR_PPRE1, RCC_ClkInitStruct->APB1CLKDivider);
  }

  if (((RCC_ClkInitStruct->ClockType) & RCC_CLOCKTYPE_PCLK2) == RCC_CLOCKTYPE_PCLK2) {
    assert_param(IS_RCC_PCLK(RCC_ClkInitStruct->APB2CLKDivider));
    MODIFY_REG(RCC->CFGR, RCC_CFGR_PPRE2, (RCC_ClkInitStruct->APB2CLKDivider) << 3U);
  }

  SystemCoreClock =
    HAL_RCC_GetSysClockFreq() >> AHBPrescTable[(RCC->CFGR & RCC_CFGR_HPRE) >> RCC_CFGR_HPRE_Pos];
  HAL_InitTick(uwTickPrio);
  return HAL_OK;
}

__weak uint32_t HAL_RCC_GetSysClockFreq(void) {
  uint32_t pllm = 0U;
  uint32_t pllvco = 0U;
  uint32_t pllp = 0U;
  uint32_t sysclockfreq = 0U;

  switch (RCC->CFGR & RCC_CFGR_SWS) {
    case RCC_CFGR_SWS_HSI:
      sysclockfreq = HSI_VALUE;
      break;
    case RCC_CFGR_SWS_HSE:
      sysclockfreq = HSE_VALUE;
      break;
    case RCC_CFGR_SWS_PLL:
      pllm = RCC->PLLCFGR & RCC_PLLCFGR_PLLM;
      if (__HAL_RCC_GET_PLL_OSCSOURCE() != RCC_PLLSOURCE_HSI) {
        pllvco = (uint32_t)((((uint64_t)HSE_VALUE) *
                              ((uint64_t)((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> RCC_PLLCFGR_PLLN_Pos))) /
                             (uint64_t)pllm);
      } else {
        pllvco = (uint32_t)((((uint64_t)HSI_VALUE) *
                              ((uint64_t)((RCC->PLLCFGR & RCC_PLLCFGR_PLLN) >> RCC_PLLCFGR_PLLN_Pos))) /
                             (uint64_t)pllm);
      }
      pllp = ((((RCC->PLLCFGR & RCC_PLLCFGR_PLLP) >> RCC_PLLCFGR_PLLP_Pos) + 1U) * 2U);
      sysclockfreq = pllvco / pllp;
      break;
    default:
      sysclockfreq = HSI_VALUE;
      break;
  }

  return sysclockfreq;
}

uint32_t HAL_RCC_GetHCLKFreq(void) {
  return SystemCoreClock;
}

#endif
