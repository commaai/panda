#include "lladc_declarations.h"

struct cadc_state_t cadc_state;
uint32_t adc_cal_factor = 1000U;

uint16_t adc_get_raw(ADC_TypeDef *adc, uint8_t channel) {
  adc->SQR1 &= ~(ADC_SQR1_L);
  adc->SQR1 = ((uint32_t) channel << 6U);

  if (channel < 10U) {
    adc->SMPR1 = (0x7U << (channel * 3U));
  } else {
    adc->SMPR2 = (0x7U << ((channel - 10U) * 3U));
  }
  adc->PCSEL_RES0 = (0x1U << channel);

  adc->CR |= ADC_CR_ADSTART;
  while (!(adc->ISR & ADC_ISR_EOC));

  uint16_t res = adc->DR;

  while (!(adc->ISR & ADC_ISR_EOS));
  adc->ISR |= ADC_ISR_EOS;

  return res;
}


uint16_t adc_get_mV(uint8_t channel) {
  return ADC_RAW_TO_mV(adc_get_raw(ADC1, channel));
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

  adc_calibrate();

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

void adc_init2(ADC_TypeDef *adc) {
  adc->CR &= ~(ADC_CR_DEEPPWD); // Reset deep-power-down mode
  adc->CR |= ADC_CR_ADVREGEN; // Enable ADC regulator
  while(!(adc->ISR & ADC_ISR_LDORDY) && (adc != ADC3));

  if (adc != ADC3) {
    adc->CR &= ~(ADC_CR_ADCALDIF); // Choose single-ended calibration
    adc->CR |= ADC_CR_ADCALLIN; // Lineriality calibration
  }
  adc->CR |= ADC_CR_ADCAL; // Start calibrtation
  while((adc->CR & ADC_CR_ADCAL) != 0);

  adc->ISR |= ADC_ISR_ADRDY;
  adc->CR |= ADC_CR_ADEN;
  while(!(adc->ISR & ADC_ISR_ADRDY));
}

void adc_calibrate(void) {
  adc_init2(ADC2);

  // make sure the channel is enabled
  ADC3_COMMON->CCR |= ADC_CCR_VREFEN;
  SYSCFG->ADC2ALT |= SYSCFG_ADC2ALT_ADC2_ROUT1;

  //uint16_t vref_raw = adc_get_raw(ADC3, 18U);
  uint32_t vref_raw = adc_get_raw(ADC2, 17U);
  print("vref_raw: "); puth(vref_raw); print("\n");

  ADC3_COMMON->CCR &= ~(ADC_CCR_VREFEN);

  // Calculate calibration factor: VREFINT typ = 1.21V
  uint32_t cal_data = *(uint16_t *)VREFINT_CAL_ADDR;
  adc_cal_factor = (3300UL * cal_data * 16UL * 10U) / (vref_raw * (current_board->avdd_mV / 100UL));
  print("cal_factor: "); puth(adc_cal_factor); print("\n");
  print("stored cal factor: "); puth(cal_data); print("\n");

  // disable ADC2 again before re-init
  ADC2->CR &= ~ADC_CR_ADEN;
}
