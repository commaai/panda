#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "platform_definitions.h"


void dac_init(DAC_TypeDef *dac, uint8_t channel, bool dma);
void dac_set(DAC_TypeDef *dac, uint8_t channel, uint32_t value);
