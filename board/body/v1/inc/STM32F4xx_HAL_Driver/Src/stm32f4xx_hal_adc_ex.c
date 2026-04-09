#include "stm32f4xx_hal.h"

#ifdef HAL_ADC_MODULE_ENABLED

HAL_StatusTypeDef HAL_ADCEx_MultiModeConfigChannel(ADC_HandleTypeDef *hadc, ADC_MultiModeTypeDef *multimode) {
  ADC_Common_TypeDef *tmpADC_Common;

  assert_param(IS_ADC_MODE(multimode->Mode));
  assert_param(IS_ADC_DMA_ACCESS_MODE(multimode->DMAAccessMode));
  assert_param(IS_ADC_SAMPLING_DELAY(multimode->TwoSamplingDelay));

  __HAL_LOCK(hadc);

  tmpADC_Common = ADC_COMMON_REGISTER(hadc);
  tmpADC_Common->CCR &= ~(ADC_CCR_MULTI);
  tmpADC_Common->CCR |= multimode->Mode;

  tmpADC_Common->CCR &= ~(ADC_CCR_DMA);
  tmpADC_Common->CCR |= multimode->DMAAccessMode;

  tmpADC_Common->CCR &= ~(ADC_CCR_DELAY);
  tmpADC_Common->CCR |= multimode->TwoSamplingDelay;

  __HAL_UNLOCK(hadc);

  return HAL_OK;
}

#endif
