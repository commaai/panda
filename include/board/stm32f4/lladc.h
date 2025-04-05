#pragma once

void register_set(volatile uint32_t *addr, uint32_t val, uint32_t mask);
void adc_init(void);
uint16_t adc_get_raw(uint8_t channel);
uint16_t adc_get_mV(uint8_t channel);
