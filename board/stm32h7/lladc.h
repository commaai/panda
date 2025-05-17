#include "lladc_declarations.h"

struct cadc_state_t cadc_state;

static uint16_t adc_get_raw(uint8_t channel) {
  uint16_t res = 0U;
  ADC1->SQR1 &= ~(ADC_SQR1_L);
  ADC1->SQR1 = (uint32_t)channel << 6U;

  ADC1->SMPR1 = 0x4UL << (channel * 3UL);
  ADC1->PCSEL_RES0 = (0x1UL << channel);
  ADC1->CFGR2 = (63UL << ADC_CFGR2_OVSR_Pos) | (0x6U << ADC_CFGR2_OVSS_Pos) | ADC_CFGR2_ROVSE;

  ADC1->CR |= ADC_CR_ADSTART;
  while (!(ADC1->ISR & ADC_ISR_EOC));

  res = ADC1->DR;

  while (!(ADC1->ISR & ADC_ISR_EOS));
  ADC1->ISR |= ADC_ISR_EOS;

  return res;
}

uint16_t adc_get_mV(uint8_t channel) {
  return ADC_RAW_TO_mV(adc_get_raw(channel));
}

void adc2_start_channel(uint8_t channel) {
  cadc_state.current_channel = channel;

  if (channel != CADC_CHANNEL_NONE) {
    ADC2->SQR1 &= ~(ADC_SQR1_L);
    ADC2->SQR1 = (uint32_t)channel << 6U;

    ADC2->SMPR1 = 0x7UL << (channel * 3UL);
    ADC2->PCSEL_RES0 = (0x1UL << channel);
    ADC2->CFGR2 = (254UL << ADC_CFGR2_OVSR_Pos) | (0x8U << ADC_CFGR2_OVSS_Pos) | ADC_CFGR2_ROVSE;

    ADC2->CR |= ADC_CR_ADSTART;
  }
}

static void ADC_IRQ_Handler(void) {
  if ((ADC2->ISR & ADC_ISR_EOC) != 0U) {
    uint16_t res = ADC2->DR;
    uint8_t next_channel = CADC_CHANNEL_NONE;

    if (cadc_state.current_channel == current_board->voltage_cadc_channel) {
      cadc_state.voltage_raw = (((CADC_FILTERING - 1U) * cadc_state.voltage_raw) + res) / CADC_FILTERING;
      next_channel = (current_board->current_cadc_channel != CADC_CHANNEL_NONE) ? current_board->current_cadc_channel : current_board->voltage_cadc_channel;
    } else if (cadc_state.current_channel == current_board->current_cadc_channel) {
      cadc_state.current_raw = (((CADC_FILTERING - 1U) * cadc_state.current_raw) + res) / CADC_FILTERING;
      next_channel = (current_board->voltage_cadc_channel != CADC_CHANNEL_NONE) ? current_board->voltage_cadc_channel : current_board->current_cadc_channel;
    } else {}

    adc2_start_channel(next_channel);
  }
}

void adc_init(void) {
  // ADC1 for one-off conversions
  ADC1->CR = ADC_CR_ADVREGEN;
  while(!(ADC1->ISR & ADC_ISR_LDORDY));

  ADC1->CR &= ~(ADC_CR_ADCALDIF);
  ADC1->CR |= ADC_CR_ADCALLIN;
  ADC1->CR |= ADC_CR_ADCAL;
  while((ADC1->CR & ADC_CR_ADCAL) != 0U);

  ADC1->ISR |= ADC_ISR_ADRDY;
  ADC1->CR |= ADC_CR_ADEN;
  while(!(ADC1->ISR & ADC_ISR_ADRDY));

  // ADC2 for continuous sampling of voltage and current
  if ((current_board->voltage_cadc_channel != CADC_CHANNEL_NONE) || (current_board->current_cadc_channel != CADC_CHANNEL_NONE)) {
    ADC2->CR = ADC_CR_ADVREGEN;
    while(!(ADC2->ISR & ADC_ISR_LDORDY));

    ADC2->CR &= ~(ADC_CR_ADCALDIF);
    ADC2->CR |= ADC_CR_ADCALLIN;
    ADC2->CR |= ADC_CR_ADCAL;
    while((ADC2->CR & ADC_CR_ADCAL) != 0U);

    ADC2->IER |= ADC_IER_EOCIE;

    ADC2->ISR |= ADC_ISR_ADRDY;
    ADC2->CR |= ADC_CR_ADEN;
    while(!(ADC2->ISR & ADC_ISR_ADRDY));

    REGISTER_INTERRUPT(ADC_IRQn, ADC_IRQ_Handler, 200U, FAULT_INTERRUPT_RATE_ADC)
    NVIC_EnableIRQ(ADC_IRQn);

    // start readout
    adc2_start_channel((current_board->voltage_cadc_channel != CADC_CHANNEL_NONE) ? current_board->voltage_cadc_channel : current_board->current_cadc_channel);
  }
}
