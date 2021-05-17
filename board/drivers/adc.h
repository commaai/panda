// ACCEL1 = ADC10
// ACCEL2 = ADC11
// VOLT_S = ADC12
// CURR_S = ADC13

#define ADCCHAN_ACCEL0 10
#define ADCCHAN_ACCEL1 11
#define ADCCHAN_VOLTAGE 12
#define ADCCHAN_CURRENT 13

void adc_init(void) {
  #ifdef STM32H7
    //register_set(&(ADC1->CCR), ADC_CCR_TSEN | ADC_CCR_VREFEN | ADC_CCR_VBATEN, 0x1C00000U); // Only present for ADC3
    register_set(&(ADC1->CR), ADC_CR_ADEN, 0x1U);
    register_set(&(ADC1->SMPR2), ADC_SMPR2_SMP12 | ADC_SMPR2_SMP13, 0xFC0U);
  #else
    register_set(&(ADC->CCR), ADC_CCR_TSVREFE | ADC_CCR_VBATE, 0xC30000U);
    register_set(&(ADC1->CR2), ADC_CR2_ADON, 0xFF7F0F03U);
    register_set(&(ADC1->SMPR1), ADC_SMPR1_SMP12 | ADC_SMPR1_SMP13, 0x7FFFFFFU);
  #endif
}

uint32_t adc_get(unsigned int channel) {
  // Select channel
  //register_set(&(ADC->JSQR), (channel << 27U), 0x3FFFFFU);

  // Temporarily disable for H7
  #ifndef STM32H7
  // Select channel
  register_set(&(ADC1->JSQR), (channel << 15U), 0x3FFFFFU);

  // Start conversion
  ADC1->SR &= ~(ADC_SR_JEOC);
  ADC1->CR2 |= ADC_CR2_JSWSTART;
  while (!(ADC1->SR & ADC_SR_JEOC));

  return ADC1->JDR1;
  #else
  return channel;
  #endif
}

uint32_t adc_get_voltage(void) {
  // REVC has a 10, 1 (1/11) voltage divider
  // Here is the calculation for the scale (s)
  // ADCV = VIN_S * (1/11) * (4095/3.3)
  // RETVAL = ADCV * s = VIN_S*1000
  // s = 1000/((4095/3.3)*(1/11)) = 8.8623046875

  // Avoid needing floating point math, so output in mV
  return (adc_get(ADCCHAN_VOLTAGE) * 8862U) / 1000U;
}
