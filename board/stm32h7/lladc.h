// 5VOUT_S = ADC12_INP5
// VOLT_S = ADC1_INP2
#define ADCCHAN_VOLTAGE 2

void adc_init(void) {
  ADC1->CR &= ~(ADC_CR_DEEPPWD); //Reset deep-power-down mode
  ADC1->CR |= ADC_CR_ADVREGEN; // Enable ADC regulator
  delay_us(100); // Let regulator to start

  ADC1->CR &= ~(ADC_CR_ADCALDIF); // Choose single-ended calibration
  ADC1->CR |= ADC_CR_ADCALLIN; // Lineriality calibration
  ADC1->CR |= ADC_CR_ADCAL; // Start calibrtation
  while((ADC1->CR & ADC_CR_ADCAL) != 0);

  ADC1->ISR |= ADC_ISR_ADRDY;
  ADC1->CR |= ADC_CR_ADEN;
  while((ADC1->ISR & ADC_ISR_ADRDY) != 1);
}

uint32_t adc_get(unsigned int channel) {

  ADC1->SQR1 &= ~(ADC_SQR1_L);
  ADC1->SQR1 = (channel << 6U);
  
  ADC1->SMPR1 = (0x7U << (channel * 3) );
  ADC1->PCSEL_RES0 = (0x1U << channel);

  ADC1->CR |= ADC_CR_ADSTART;
  while (!(ADC1->ISR & ADC_ISR_EOC));

  uint16_t res = ADC1->DR;

  while (!(ADC1->ISR & ADC_ISR_EOS));
  ADC1->ISR |= ADC_ISR_EOS;

  return res;
}
// FIXME: fix calculations on the comments
uint32_t adc_get_voltage(void) {
  // REVC has a 10, 1 (1/11) voltage divider
  // Here is the calculation for the scale (s)
  // ADCV = VIN_S * (1/11) * (4095/3.3)
  // RETVAL = ADCV * s = VIN_S*1000
  // s = 1000/((4095/3.3)*(1/11)) = 8.8623046875

  // Avoid needing floating point math, so output in mV
  return ((adc_get(ADCCHAN_VOLTAGE) * 1000U) / 19893) * 11;
}
