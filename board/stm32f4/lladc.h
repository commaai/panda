#include "lladc_declarations.h"

void register_set(volatile uint32_t *addr, uint32_t val, uint32_t mask);

void adc_init(ADC_TypeDef *adc) {
  register_set(&(ADC->CCR), ADC_CCR_TSVREFE | ADC_CCR_VBATE, 0xC30000U);
  register_set(&(adc->CR2), ADC_CR2_ADON, 0xFF7F0F03U);
}

static uint16_t adc_get_raw(const adc_signal_t *signal) {
  // Set sampling time
  if (signal->channel < 10U) {
    register_set(&(signal->adc->SMPR2), (uint32_t)signal->sample_time << (signal->channel * 3U), 0x7FFFFFFU);
  } else {
    register_set(&(signal->adc->SMPR1), (uint32_t)signal->sample_time << ((signal->channel - 10U) * 3U), 0x7FFFFFFU);
  }

  // Select channel
  register_set(&(signal->adc->JSQR), ((uint32_t) signal->channel << 15U), 0x3FFFFFU);

  // Start conversion
  signal->adc->SR &= ~(ADC_SR_JEOC);
  signal->adc->CR2 |= ADC_CR2_JSWSTART;
  while (!(signal->adc->SR & ADC_SR_JEOC));

  return signal->adc->JDR1;
}

uint16_t adc_get_mV(const adc_signal_t *signal) {
  return (adc_get_raw(signal) * current_board->avdd_mV) / 4095U;
}
