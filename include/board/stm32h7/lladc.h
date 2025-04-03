#pragma once
#include <stdint.h>

void adc_init(void);
uint16_t adc_get_mV(uint8_t channel);
