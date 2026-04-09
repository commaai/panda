#include "stm32f4xx_hal.h"

#ifdef HAL_ADC_MODULE_ENABLED

static void ADC_Init(ADC_HandleTypeDef *hadc);

HAL_StatusTypeDef HAL_ADC_Init(ADC_HandleTypeDef *hadc) {
  HAL_StatusTypeDef tmp_hal_status = HAL_OK;

  if (hadc == NULL) {
    return HAL_ERROR;
  }

  assert_param(IS_ADC_ALL_INSTANCE(hadc->Instance));
  assert_param(IS_ADC_CLOCKPRESCALER(hadc->Init.ClockPrescaler));
  assert_param(IS_ADC_RESOLUTION(hadc->Init.Resolution));
  assert_param(IS_FUNCTIONAL_STATE(hadc->Init.ScanConvMode));
  assert_param(IS_FUNCTIONAL_STATE(hadc->Init.ContinuousConvMode));
  assert_param(IS_ADC_EXT_TRIG(hadc->Init.ExternalTrigConv));
  assert_param(IS_ADC_DATA_ALIGN(hadc->Init.DataAlign));
  assert_param(IS_ADC_REGULAR_LENGTH(hadc->Init.NbrOfConversion));
  assert_param(IS_FUNCTIONAL_STATE(hadc->Init.DMAContinuousRequests));
  assert_param(IS_ADC_EOCSelection(hadc->Init.EOCSelection));
  assert_param(IS_FUNCTIONAL_STATE(hadc->Init.DiscontinuousConvMode));

  if (hadc->Init.ExternalTrigConv != ADC_SOFTWARE_START) {
    assert_param(IS_ADC_EXT_TRIG_EDGE(hadc->Init.ExternalTrigConvEdge));
  }

  if (hadc->State == HAL_ADC_STATE_RESET) {
#if (USE_HAL_ADC_REGISTER_CALLBACKS == 1)
    hadc->ConvCpltCallback = HAL_ADC_ConvCpltCallback;
    hadc->ConvHalfCpltCallback = HAL_ADC_ConvHalfCpltCallback;
    hadc->LevelOutOfWindowCallback = HAL_ADC_LevelOutOfWindowCallback;
    hadc->ErrorCallback = HAL_ADC_ErrorCallback;
    hadc->InjectedConvCpltCallback = HAL_ADCEx_InjectedConvCpltCallback;
    if (hadc->MspInitCallback == NULL) {
      hadc->MspInitCallback = HAL_ADC_MspInit;
    }
    hadc->MspInitCallback(hadc);
#else
    HAL_ADC_MspInit(hadc);
#endif
    ADC_CLEAR_ERRORCODE(hadc);
    hadc->Lock = HAL_UNLOCKED;
  }

  if (HAL_IS_BIT_CLR(hadc->State, HAL_ADC_STATE_ERROR_INTERNAL)) {
    ADC_STATE_CLR_SET(hadc->State,
                      HAL_ADC_STATE_REG_BUSY | HAL_ADC_STATE_INJ_BUSY,
                      HAL_ADC_STATE_BUSY_INTERNAL);
    ADC_Init(hadc);
    ADC_CLEAR_ERRORCODE(hadc);
    ADC_STATE_CLR_SET(hadc->State, HAL_ADC_STATE_BUSY_INTERNAL, HAL_ADC_STATE_READY);
  } else {
    tmp_hal_status = HAL_ERROR;
  }

  __HAL_UNLOCK(hadc);
  return tmp_hal_status;
}

__weak void HAL_ADC_MspInit(ADC_HandleTypeDef *hadc) {
  UNUSED(hadc);
}

HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *hadc) {
  __IO uint32_t counter = 0U;
  ADC_Common_TypeDef *tmpADC_Common;

  assert_param(IS_FUNCTIONAL_STATE(hadc->Init.ContinuousConvMode));
  assert_param(IS_ADC_EXT_TRIG_EDGE(hadc->Init.ExternalTrigConvEdge));

  __HAL_LOCK(hadc);

  if ((hadc->Instance->CR2 & ADC_CR2_ADON) != ADC_CR2_ADON) {
    __HAL_ADC_ENABLE(hadc);

    counter = (ADC_STAB_DELAY_US * (SystemCoreClock / 1000000U));
    while (counter != 0U) {
      counter--;
    }
  }

  if (HAL_IS_BIT_SET(hadc->Instance->CR2, ADC_CR2_ADON)) {
    ADC_STATE_CLR_SET(hadc->State,
                      HAL_ADC_STATE_READY | HAL_ADC_STATE_REG_EOC | HAL_ADC_STATE_REG_OVR,
                      HAL_ADC_STATE_REG_BUSY);

    if (READ_BIT(hadc->Instance->CR1, ADC_CR1_JAUTO) != RESET) {
      ADC_STATE_CLR_SET(hadc->State, HAL_ADC_STATE_INJ_EOC, HAL_ADC_STATE_INJ_BUSY);
    }

    if (HAL_IS_BIT_SET(hadc->State, HAL_ADC_STATE_INJ_BUSY)) {
      CLEAR_BIT(hadc->ErrorCode, (HAL_ADC_ERROR_OVR | HAL_ADC_ERROR_DMA));
    } else {
      ADC_CLEAR_ERRORCODE(hadc);
    }

    __HAL_UNLOCK(hadc);

    tmpADC_Common = ADC_COMMON_REGISTER(hadc);
    __HAL_ADC_CLEAR_FLAG(hadc, ADC_FLAG_EOC | ADC_FLAG_OVR);

    if (HAL_IS_BIT_CLR(tmpADC_Common->CCR, ADC_CCR_MULTI)) {
#if defined(ADC2) && defined(ADC3)
      if ((hadc->Instance == ADC1) ||
          ((hadc->Instance == ADC2) && ((ADC->CCR & ADC_CCR_MULTI_Msk) < ADC_CCR_MULTI_0)) ||
          ((hadc->Instance == ADC3) && ((ADC->CCR & ADC_CCR_MULTI_Msk) < ADC_CCR_MULTI_4))) {
#endif
        if ((hadc->Instance->CR2 & ADC_CR2_EXTEN) == RESET) {
          hadc->Instance->CR2 |= (uint32_t)ADC_CR2_SWSTART;
        }
#if defined(ADC2) && defined(ADC3)
      }
#endif
    } else {
      if ((hadc->Instance == ADC1) && ((hadc->Instance->CR2 & ADC_CR2_EXTEN) == RESET)) {
        hadc->Instance->CR2 |= (uint32_t)ADC_CR2_SWSTART;
      }
    }
  } else {
    SET_BIT(hadc->State, HAL_ADC_STATE_ERROR_INTERNAL);
    SET_BIT(hadc->ErrorCode, HAL_ADC_ERROR_INTERNAL);
  }

  return HAL_OK;
}

HAL_StatusTypeDef HAL_ADC_ConfigChannel(ADC_HandleTypeDef *hadc, ADC_ChannelConfTypeDef *sConfig) {
  __IO uint32_t counter = 0U;
  ADC_Common_TypeDef *tmpADC_Common;

  assert_param(IS_ADC_CHANNEL(sConfig->Channel));
  assert_param(IS_ADC_REGULAR_RANK(sConfig->Rank));
  assert_param(IS_ADC_SAMPLE_TIME(sConfig->SamplingTime));

  __HAL_LOCK(hadc);

  if (sConfig->Channel > ADC_CHANNEL_9) {
    hadc->Instance->SMPR1 &= ~ADC_SMPR1(ADC_SMPR1_SMP10, sConfig->Channel);
    hadc->Instance->SMPR1 |= ADC_SMPR1(sConfig->SamplingTime, sConfig->Channel);
  } else {
    hadc->Instance->SMPR2 &= ~ADC_SMPR2(ADC_SMPR2_SMP0, sConfig->Channel);
    hadc->Instance->SMPR2 |= ADC_SMPR2(sConfig->SamplingTime, sConfig->Channel);
  }

  if (sConfig->Rank < 7U) {
    hadc->Instance->SQR3 &= ~ADC_SQR3_RK(ADC_SQR3_SQ1, sConfig->Rank);
    hadc->Instance->SQR3 |= ADC_SQR3_RK(sConfig->Channel, sConfig->Rank);
  } else if (sConfig->Rank < 13U) {
    hadc->Instance->SQR2 &= ~ADC_SQR2_RK(ADC_SQR2_SQ7, sConfig->Rank);
    hadc->Instance->SQR2 |= ADC_SQR2_RK(sConfig->Channel, sConfig->Rank);
  } else {
    hadc->Instance->SQR1 &= ~ADC_SQR1_RK(ADC_SQR1_SQ13, sConfig->Rank);
    hadc->Instance->SQR1 |= ADC_SQR1_RK(sConfig->Channel, sConfig->Rank);
  }

  tmpADC_Common = ADC_COMMON_REGISTER(hadc);

  if ((hadc->Instance == ADC1) && (sConfig->Channel == ADC_CHANNEL_VBAT)) {
    if ((uint16_t)ADC_CHANNEL_TEMPSENSOR == (uint16_t)ADC_CHANNEL_VBAT) {
      tmpADC_Common->CCR &= ~ADC_CCR_TSVREFE;
    }
    tmpADC_Common->CCR |= ADC_CCR_VBATE;
  }

  if ((hadc->Instance == ADC1) &&
      ((sConfig->Channel == ADC_CHANNEL_TEMPSENSOR) || (sConfig->Channel == ADC_CHANNEL_VREFINT))) {
    if ((uint16_t)ADC_CHANNEL_TEMPSENSOR == (uint16_t)ADC_CHANNEL_VBAT) {
      tmpADC_Common->CCR &= ~ADC_CCR_VBATE;
    }
    tmpADC_Common->CCR |= ADC_CCR_TSVREFE;

    if (sConfig->Channel == ADC_CHANNEL_TEMPSENSOR) {
      counter = (ADC_TEMPSENSOR_DELAY_US * (SystemCoreClock / 1000000U));
      while (counter != 0U) {
        counter--;
      }
    }
  }

  __HAL_UNLOCK(hadc);
  return HAL_OK;
}

static void ADC_Init(ADC_HandleTypeDef *hadc) {
  ADC_Common_TypeDef *tmpADC_Common;

  tmpADC_Common = ADC_COMMON_REGISTER(hadc);

  tmpADC_Common->CCR &= ~(ADC_CCR_ADCPRE);
  tmpADC_Common->CCR |= hadc->Init.ClockPrescaler;

  hadc->Instance->CR1 &= ~(ADC_CR1_SCAN);
  hadc->Instance->CR1 |= ADC_CR1_SCANCONV(hadc->Init.ScanConvMode);

  hadc->Instance->CR1 &= ~(ADC_CR1_RES);
  hadc->Instance->CR1 |= hadc->Init.Resolution;

  hadc->Instance->CR2 &= ~(ADC_CR2_ALIGN);
  hadc->Instance->CR2 |= hadc->Init.DataAlign;

  if (hadc->Init.ExternalTrigConv != ADC_SOFTWARE_START) {
    hadc->Instance->CR2 &= ~(ADC_CR2_EXTSEL);
    hadc->Instance->CR2 |= hadc->Init.ExternalTrigConv;

    hadc->Instance->CR2 &= ~(ADC_CR2_EXTEN);
    hadc->Instance->CR2 |= hadc->Init.ExternalTrigConvEdge;
  } else {
    hadc->Instance->CR2 &= ~(ADC_CR2_EXTSEL);
    hadc->Instance->CR2 &= ~(ADC_CR2_EXTEN);
  }

  hadc->Instance->CR2 &= ~(ADC_CR2_CONT);
  hadc->Instance->CR2 |= ADC_CR2_CONTINUOUS((uint32_t)hadc->Init.ContinuousConvMode);

  if (hadc->Init.DiscontinuousConvMode != DISABLE) {
    assert_param(IS_ADC_REGULAR_DISC_NUMBER(hadc->Init.NbrOfDiscConversion));
    hadc->Instance->CR1 |= (uint32_t)ADC_CR1_DISCEN;
    hadc->Instance->CR1 &= ~(ADC_CR1_DISCNUM);
    hadc->Instance->CR1 |= ADC_CR1_DISCONTINUOUS(hadc->Init.NbrOfDiscConversion);
  } else {
    hadc->Instance->CR1 &= ~(ADC_CR1_DISCEN);
  }

  hadc->Instance->SQR1 &= ~(ADC_SQR1_L);
  hadc->Instance->SQR1 |= ADC_SQR1(hadc->Init.NbrOfConversion);

  hadc->Instance->CR2 &= ~(ADC_CR2_DDS);
  hadc->Instance->CR2 |= ADC_CR2_DMAContReq((uint32_t)hadc->Init.DMAContinuousRequests);

  hadc->Instance->CR2 &= ~(ADC_CR2_EOCS);
  hadc->Instance->CR2 |= ADC_CR2_EOCSelection(hadc->Init.EOCSelection);
}

#endif
