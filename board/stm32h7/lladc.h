#include "lladc_declarations.h"

void adc_init(ADC_TypeDef *adc) {
  adc->CR &= ~(ADC_CR_DEEPPWD); // Reset deep-power-down mode
  adc->CR |= ADC_CR_ADVREGEN; // Enable ADC regulator
  while(!(adc->ISR & ADC_ISR_LDORDY) && (adc != ADC3));

  if (adc != ADC3) {
    adc->CR &= ~(ADC_CR_ADCALDIF); // Choose single-ended calibration
    adc->CR |= ADC_CR_ADCALLIN; // Lineriality calibration
  }
  adc->CR |= ADC_CR_ADCAL; // Start calibrtation
  while((adc->CR & ADC_CR_ADCAL) != 0U);

  adc->ISR |= ADC_ISR_ADRDY;
  adc->CR |= ADC_CR_ADEN;
  while(!(adc->ISR & ADC_ISR_ADRDY));
}

uint16_t adc_get_raw(const adc_signal_t *signal) {
  signal->adc->SQR1 &= ~(ADC_SQR1_L);
  signal->adc->SQR1 = ((uint32_t) signal->channel << 6U);

  // sample time
  if (signal->channel < 10U) {
    signal->adc->SMPR1 = ((uint32_t) signal->sample_time << (signal->channel * 3U));
  } else {
    signal->adc->SMPR2 = ((uint32_t) signal->sample_time << ((signal->channel - 10U) * 3U));
  }

  // select channel
  signal->adc->PCSEL_RES0 = (0x1UL << signal->channel);

  // oversampling
  signal->adc->CFGR2 = (((1U << (uint32_t) signal->oversampling) - 1U) << ADC_CFGR2_OVSR_Pos) | ((uint32_t) signal->oversampling << ADC_CFGR2_OVSS_Pos) | ADC_CFGR2_ROVSE;

  // start conversion
  signal->adc->CR |= ADC_CR_ADSTART;
  while (!(signal->adc->ISR & ADC_ISR_EOC));

  uint16_t res = signal->adc->DR;

  while (!(signal->adc->ISR & ADC_ISR_EOS));
  signal->adc->ISR |= ADC_ISR_EOS;

  return res;
}

uint16_t adc_get_mV(const adc_signal_t *signal) {
  uint16_t ret = 0;
  if ((signal->adc == ADC1) || (signal->adc == ADC2)) {
    ret = (adc_get_raw(signal) * current_board->avdd_mV) / 65535U;
  } else if (signal->adc == ADC3) {
    ret = (adc_get_raw(signal) * current_board->avdd_mV) / 4095U;
  } else {}
  return ret;
}