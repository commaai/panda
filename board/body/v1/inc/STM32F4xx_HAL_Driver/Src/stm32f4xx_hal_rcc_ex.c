#include "stm32f4xx_hal.h"

#ifdef HAL_RCC_MODULE_ENABLED

uint32_t HAL_RCC_GetSysClockFreq(void) {
  uint32_t pllm = 0U;
  uint32_t pllvco = 0U;
  uint32_t pllp = 0U;
#if defined(RCC_CFGR_SWS_PLLR)
  uint32_t pllr = 0U;
#endif
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
#if defined(RCC_CFGR_SWS_PLLR)
    case RCC_CFGR_SWS_PLLR:
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
      pllr = ((RCC->PLLCFGR & RCC_PLLCFGR_PLLR) >> RCC_PLLCFGR_PLLR_Pos);
      sysclockfreq = pllvco / pllr;
      break;
#endif
    default:
      sysclockfreq = HSI_VALUE;
      break;
  }

  return sysclockfreq;
}

HAL_StatusTypeDef HAL_RCC_OscConfig(RCC_OscInitTypeDef *RCC_OscInitStruct) {
  uint32_t tickstart, pll_config;

  if (RCC_OscInitStruct == NULL) {
    return HAL_ERROR;
  }

  assert_param(IS_RCC_OSCILLATORTYPE(RCC_OscInitStruct->OscillatorType));

  if (((RCC_OscInitStruct->OscillatorType) & RCC_OSCILLATORTYPE_HSE) == RCC_OSCILLATORTYPE_HSE) {
    assert_param(IS_RCC_HSE(RCC_OscInitStruct->HSEState));
#if defined(STM32F446xx)
    if ((__HAL_RCC_GET_SYSCLK_SOURCE() == RCC_CFGR_SWS_HSE) ||
        ((__HAL_RCC_GET_SYSCLK_SOURCE() == RCC_CFGR_SWS_PLL) &&
         ((RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC) == RCC_PLLCFGR_PLLSRC_HSE)) ||
        ((__HAL_RCC_GET_SYSCLK_SOURCE() == RCC_CFGR_SWS_PLLR) &&
         ((RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC) == RCC_PLLCFGR_PLLSRC_HSE)))
#else
    if ((__HAL_RCC_GET_SYSCLK_SOURCE() == RCC_CFGR_SWS_HSE) ||
        ((__HAL_RCC_GET_SYSCLK_SOURCE() == RCC_CFGR_SWS_PLL) &&
         ((RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC) == RCC_PLLCFGR_PLLSRC_HSE)))
#endif
    {
      if ((__HAL_RCC_GET_FLAG(RCC_FLAG_HSERDY) != RESET) &&
          (RCC_OscInitStruct->HSEState == RCC_HSE_OFF)) {
        return HAL_ERROR;
      }
    } else {
      __HAL_RCC_HSE_CONFIG(RCC_OscInitStruct->HSEState);

      if ((RCC_OscInitStruct->HSEState) != RCC_HSE_OFF) {
        tickstart = HAL_GetTick();
        while (__HAL_RCC_GET_FLAG(RCC_FLAG_HSERDY) == RESET) {
          if ((HAL_GetTick() - tickstart) > HSE_TIMEOUT_VALUE) {
            return HAL_TIMEOUT;
          }
        }
      } else {
        tickstart = HAL_GetTick();
        while (__HAL_RCC_GET_FLAG(RCC_FLAG_HSERDY) != RESET) {
          if ((HAL_GetTick() - tickstart) > HSE_TIMEOUT_VALUE) {
            return HAL_TIMEOUT;
          }
        }
      }
    }
  }

  if (((RCC_OscInitStruct->OscillatorType) & RCC_OSCILLATORTYPE_HSI) == RCC_OSCILLATORTYPE_HSI) {
    assert_param(IS_RCC_HSI(RCC_OscInitStruct->HSIState));
    assert_param(IS_RCC_CALIBRATION_VALUE(RCC_OscInitStruct->HSICalibrationValue));
#if defined(STM32F446xx)
    if ((__HAL_RCC_GET_SYSCLK_SOURCE() == RCC_CFGR_SWS_HSI) ||
        ((__HAL_RCC_GET_SYSCLK_SOURCE() == RCC_CFGR_SWS_PLL) &&
         ((RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC) == RCC_PLLCFGR_PLLSRC_HSI)) ||
        ((__HAL_RCC_GET_SYSCLK_SOURCE() == RCC_CFGR_SWS_PLLR) &&
         ((RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC) == RCC_PLLCFGR_PLLSRC_HSI)))
#else
    if ((__HAL_RCC_GET_SYSCLK_SOURCE() == RCC_CFGR_SWS_HSI) ||
        ((__HAL_RCC_GET_SYSCLK_SOURCE() == RCC_CFGR_SWS_PLL) &&
         ((RCC->PLLCFGR & RCC_PLLCFGR_PLLSRC) == RCC_PLLCFGR_PLLSRC_HSI)))
#endif
    {
      if ((__HAL_RCC_GET_FLAG(RCC_FLAG_HSIRDY) != RESET) &&
          (RCC_OscInitStruct->HSIState != RCC_HSI_ON)) {
        return HAL_ERROR;
      } else {
        __HAL_RCC_HSI_CALIBRATIONVALUE_ADJUST(RCC_OscInitStruct->HSICalibrationValue);
      }
    } else {
      if ((RCC_OscInitStruct->HSIState) != RCC_HSI_OFF) {
        __HAL_RCC_HSI_ENABLE();
        tickstart = HAL_GetTick();
        while (__HAL_RCC_GET_FLAG(RCC_FLAG_HSIRDY) == RESET) {
          if ((HAL_GetTick() - tickstart) > HSI_TIMEOUT_VALUE) {
            return HAL_TIMEOUT;
          }
        }
        __HAL_RCC_HSI_CALIBRATIONVALUE_ADJUST(RCC_OscInitStruct->HSICalibrationValue);
      } else {
        __HAL_RCC_HSI_DISABLE();
        tickstart = HAL_GetTick();
        while (__HAL_RCC_GET_FLAG(RCC_FLAG_HSIRDY) != RESET) {
          if ((HAL_GetTick() - tickstart) > HSI_TIMEOUT_VALUE) {
            return HAL_TIMEOUT;
          }
        }
      }
    }
  }

  if (((RCC_OscInitStruct->OscillatorType) & RCC_OSCILLATORTYPE_LSI) == RCC_OSCILLATORTYPE_LSI) {
    assert_param(IS_RCC_LSI(RCC_OscInitStruct->LSIState));

    if ((RCC_OscInitStruct->LSIState) != RCC_LSI_OFF) {
      __HAL_RCC_LSI_ENABLE();
      tickstart = HAL_GetTick();
      while (__HAL_RCC_GET_FLAG(RCC_FLAG_LSIRDY) == RESET) {
        if ((HAL_GetTick() - tickstart) > LSI_TIMEOUT_VALUE) {
          return HAL_TIMEOUT;
        }
      }
    } else {
      __HAL_RCC_LSI_DISABLE();
      tickstart = HAL_GetTick();
      while (__HAL_RCC_GET_FLAG(RCC_FLAG_LSIRDY) != RESET) {
        if ((HAL_GetTick() - tickstart) > LSI_TIMEOUT_VALUE) {
          return HAL_TIMEOUT;
        }
      }
    }
  }

  if (((RCC_OscInitStruct->OscillatorType) & RCC_OSCILLATORTYPE_LSE) == RCC_OSCILLATORTYPE_LSE) {
    FlagStatus pwrclkchanged = RESET;

    assert_param(IS_RCC_LSE(RCC_OscInitStruct->LSEState));

    if (__HAL_RCC_PWR_IS_CLK_DISABLED()) {
      __HAL_RCC_PWR_CLK_ENABLE();
      pwrclkchanged = SET;
    }

    if (HAL_IS_BIT_CLR(PWR->CR, PWR_CR_DBP)) {
      SET_BIT(PWR->CR, PWR_CR_DBP);
      tickstart = HAL_GetTick();
      while (HAL_IS_BIT_CLR(PWR->CR, PWR_CR_DBP)) {
        if ((HAL_GetTick() - tickstart) > RCC_DBP_TIMEOUT_VALUE) {
          return HAL_TIMEOUT;
        }
      }
    }

    __HAL_RCC_LSE_CONFIG(RCC_OscInitStruct->LSEState);
    if ((RCC_OscInitStruct->LSEState) != RCC_LSE_OFF) {
      tickstart = HAL_GetTick();
      while (__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) == RESET) {
        if ((HAL_GetTick() - tickstart) > RCC_LSE_TIMEOUT_VALUE) {
          return HAL_TIMEOUT;
        }
      }
    } else {
      tickstart = HAL_GetTick();
      while (__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) != RESET) {
        if ((HAL_GetTick() - tickstart) > RCC_LSE_TIMEOUT_VALUE) {
          return HAL_TIMEOUT;
        }
      }
    }

    if (pwrclkchanged == SET) {
      __HAL_RCC_PWR_CLK_DISABLE();
    }
  }

  assert_param(IS_RCC_PLL(RCC_OscInitStruct->PLL.PLLState));
  if ((RCC_OscInitStruct->PLL.PLLState) != RCC_PLL_NONE) {
    if (__HAL_RCC_GET_SYSCLK_SOURCE() != RCC_CFGR_SWS_PLL) {
      if ((RCC_OscInitStruct->PLL.PLLState) == RCC_PLL_ON) {
        assert_param(IS_RCC_PLLSOURCE(RCC_OscInitStruct->PLL.PLLSource));
        assert_param(IS_RCC_PLLM_VALUE(RCC_OscInitStruct->PLL.PLLM));
        assert_param(IS_RCC_PLLN_VALUE(RCC_OscInitStruct->PLL.PLLN));
        assert_param(IS_RCC_PLLP_VALUE(RCC_OscInitStruct->PLL.PLLP));
        assert_param(IS_RCC_PLLQ_VALUE(RCC_OscInitStruct->PLL.PLLQ));
        assert_param(IS_RCC_PLLR_VALUE(RCC_OscInitStruct->PLL.PLLR));

        __HAL_RCC_PLL_DISABLE();
        tickstart = HAL_GetTick();
        while (__HAL_RCC_GET_FLAG(RCC_FLAG_PLLRDY) != RESET) {
          if ((HAL_GetTick() - tickstart) > PLL_TIMEOUT_VALUE) {
            return HAL_TIMEOUT;
          }
        }

        WRITE_REG(RCC->PLLCFGR,
                  (RCC_OscInitStruct->PLL.PLLSource |
                   RCC_OscInitStruct->PLL.PLLM |
                   (RCC_OscInitStruct->PLL.PLLN << RCC_PLLCFGR_PLLN_Pos) |
                   (((RCC_OscInitStruct->PLL.PLLP >> 1U) - 1U) << RCC_PLLCFGR_PLLP_Pos) |
                   (RCC_OscInitStruct->PLL.PLLQ << RCC_PLLCFGR_PLLQ_Pos) |
                   (RCC_OscInitStruct->PLL.PLLR << RCC_PLLCFGR_PLLR_Pos)));

        __HAL_RCC_PLL_ENABLE();
        tickstart = HAL_GetTick();
        while (__HAL_RCC_GET_FLAG(RCC_FLAG_PLLRDY) == RESET) {
          if ((HAL_GetTick() - tickstart) > PLL_TIMEOUT_VALUE) {
            return HAL_TIMEOUT;
          }
        }
      } else {
        __HAL_RCC_PLL_DISABLE();
        tickstart = HAL_GetTick();
        while (__HAL_RCC_GET_FLAG(RCC_FLAG_PLLRDY) != RESET) {
          if ((HAL_GetTick() - tickstart) > PLL_TIMEOUT_VALUE) {
            return HAL_TIMEOUT;
          }
        }
      }
    } else {
      if ((RCC_OscInitStruct->PLL.PLLState) == RCC_PLL_OFF) {
        return HAL_ERROR;
      } else {
        pll_config = RCC->PLLCFGR;
#if defined(RCC_PLLCFGR_PLLR)
        if (((RCC_OscInitStruct->PLL.PLLState) == RCC_PLL_OFF) ||
            (READ_BIT(pll_config, RCC_PLLCFGR_PLLSRC) != RCC_OscInitStruct->PLL.PLLSource) ||
            (READ_BIT(pll_config, RCC_PLLCFGR_PLLM) !=
             (RCC_OscInitStruct->PLL.PLLM) << RCC_PLLCFGR_PLLM_Pos) ||
            (READ_BIT(pll_config, RCC_PLLCFGR_PLLN) !=
             (RCC_OscInitStruct->PLL.PLLN) << RCC_PLLCFGR_PLLN_Pos) ||
            (READ_BIT(pll_config, RCC_PLLCFGR_PLLP) !=
             (((RCC_OscInitStruct->PLL.PLLP >> 1U) - 1U)) << RCC_PLLCFGR_PLLP_Pos) ||
            (READ_BIT(pll_config, RCC_PLLCFGR_PLLQ) !=
             (RCC_OscInitStruct->PLL.PLLQ << RCC_PLLCFGR_PLLQ_Pos)) ||
            (READ_BIT(pll_config, RCC_PLLCFGR_PLLR) !=
             (RCC_OscInitStruct->PLL.PLLR << RCC_PLLCFGR_PLLR_Pos)))
#else
        if (((RCC_OscInitStruct->PLL.PLLState) == RCC_PLL_OFF) ||
            (READ_BIT(pll_config, RCC_PLLCFGR_PLLSRC) != RCC_OscInitStruct->PLL.PLLSource) ||
            (READ_BIT(pll_config, RCC_PLLCFGR_PLLM) !=
             (RCC_OscInitStruct->PLL.PLLM) << RCC_PLLCFGR_PLLM_Pos) ||
            (READ_BIT(pll_config, RCC_PLLCFGR_PLLN) !=
             (RCC_OscInitStruct->PLL.PLLN) << RCC_PLLCFGR_PLLN_Pos) ||
            (READ_BIT(pll_config, RCC_PLLCFGR_PLLP) !=
             (((RCC_OscInitStruct->PLL.PLLP >> 1U) - 1U)) << RCC_PLLCFGR_PLLP_Pos) ||
            (READ_BIT(pll_config, RCC_PLLCFGR_PLLQ) !=
             (RCC_OscInitStruct->PLL.PLLQ << RCC_PLLCFGR_PLLQ_Pos)))
#endif
        {
          return HAL_ERROR;
        }
      }
    }
  }

  return HAL_OK;
}

#endif
