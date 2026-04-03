#pragma once

#include "lladc_declarations.h"

void adc_init(ADC_TypeDef *adc);
uint16_t adc_get_raw(const adc_signal_t *signal);
uint16_t adc_get_mV(const adc_signal_t *signal);
