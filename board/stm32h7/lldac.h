#pragma once

void dac_init(DAC_TypeDef *dac, uint8_t channel, bool dma);

// Set channel 1 value, in mV
void dac_set(DAC_TypeDef *dac, uint8_t channel, uint32_t value);
