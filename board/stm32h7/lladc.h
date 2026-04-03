#ifndef PANDA_STM32H7_LLADC_H
#define PANDA_STM32H7_LLADC_H

#include <stdint.h>
#include "lladc_declarations.h"

void adc_init(ADC_TypeDef *adc);
uint16_t adc_get_raw(const adc_signal_t *signal);
uint16_t adc_get_mV(const adc_signal_t *signal);

#endif
