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
  return (adc_get_raw(channel) * current_board->avdd_mV) / 65535U;
}

uint32_t voltage_raw = 0U;
uint32_t current_raw = 0U;
bool reading_voltage = true;

void adc2_start_channel(uint8_t channel) {
  ADC2->SQR1 &= ~(ADC_SQR1_L);
  ADC2->SQR1 = (uint32_t)channel << 6U;

  ADC2->SMPR1 = 0x7UL << (channel * 3UL);
  ADC2->PCSEL_RES0 = (0x1UL << channel);
  ADC2->CFGR2 = (254UL << ADC_CFGR2_OVSR_Pos) | (0x8U << ADC_CFGR2_OVSS_Pos) | ADC_CFGR2_ROVSE;

  ADC2->CR |= ADC_CR_ADSTART;
}

static void ADC_IRQ_Handler(void) {
  if (ADC2->ISR & ADC_ISR_EOC) {
    uint16_t res = ADC2->DR;
    uint8_t next_channel = 0U;

    if (reading_voltage) {
      voltage_raw =  ((7U * voltage_raw) + res) / 8U;
      next_channel = 3U;
      reading_voltage = false;
    } else {
      current_raw = ((7U * current_raw) + res) / 8U;
      next_channel = 8U;
      reading_voltage = true;
    }

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

  // start voltage readout
  adc2_start_channel(8U);

  REGISTER_INTERRUPT(ADC_IRQn, ADC_IRQ_Handler, 200U, FAULT_INTERRUPT_RATE_ADC)
  NVIC_EnableIRQ(ADC_IRQn);
}
