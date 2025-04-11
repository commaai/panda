#pragma once
#include "platform_definitions.h"

typedef struct {
  ADC_TypeDef *adc;
  uint8_t channel;
} adc_channel_t;

void adc_init(ADC_TypeDef *adc);
uint16_t adc_get_raw(ADC_TypeDef *adc, uint8_t channel);
uint16_t adc_get_mV(ADC_TypeDef *adc, uint8_t channel);
