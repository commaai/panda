// ACCEL1 = ADC10
// ACCEL2 = ADC11
// VOLT_S = ADC12
// CURR_S = ADC13
// 5VOUT_S = ADC14

// for red panda:
// 5VOUT_S = ADC1_INP2
// VOLT_S = ADC2_INP6

#define ADCCHAN_ACCEL0 10
#define ADCCHAN_ACCEL1 11
//#define ADCCHAN_VOLTAGE 12
#define ADCCHAN_CURRENT 13

#define ADCCHAN_VOLTAGE 6

void adc_init(void) {
  #ifdef STM32H7

    ADC2->CR &= ~(ADC_CR_DEEPPWD); //Reset deep-power-down mode
    ADC2->CR |= ADC_CR_ADVREGEN; // Enable ADC regulator
    delay_us(100); // Let regulator to start

    ADC2->CR &= ~(ADC_CR_ADCALDIF); // Choose single-ended calibration
    ADC2->CR |= ADC_CR_ADCALLIN; // Lineriality calibration
    ADC2->CR |= ADC_CR_ADCAL; // Start calibrtation
    while((ADC2->CR & ADC_CR_ADCAL) != 0);

    ADC2->ISR |= ADC_ISR_ADRDY;
    ADC2->CR |= ADC_CR_ADEN;
    while((ADC2->ISR & ADC_ISR_ADRDY) != 1);

  #else
    register_set(&(ADC->CCR), ADC_CCR_TSVREFE | ADC_CCR_VBATE, 0xC30000U); // BUG: VBATE must be disabled when TSVREFE is set. If both bits are set, only the VBAT conversion is performed. Also why???
    register_set(&(ADC1->CR2), ADC_CR2_ADON, 0xFF7F0F03U);
    register_set(&(ADC1->SMPR1), ADC_SMPR1_SMP12 | ADC_SMPR1_SMP13, 0x7FFFFFFU);
  #endif
}

uint32_t adc_get(unsigned int channel) {

  ADC2->SQR1 &= ~(ADC_SQR1_L);
  ADC2->SQR1 |= (channel << 6U);
  
  ADC2->SMPR1 |= (0x7U << (channel * 3) );
  ADC2->PCSEL_RES0 |= (0x1U << channel);

  ADC2->CR |= ADC_CR_ADSTART;
  while (!(ADC2->ISR & ADC_ISR_EOC));

  uint16_t res = ADC2->DR;

  while (!(ADC2->ISR & ADC_ISR_EOS));
  ADC2->ISR |= ADC_ISR_EOS;

  return res;
}

uint32_t adc_get_voltage(void) {
  // REVC has a 10, 1 (1/11) voltage divider
  // Here is the calculation for the scale (s)
  // ADCV = VIN_S * (1/11) * (4095/3.3)
  // RETVAL = ADCV * s = VIN_S*1000
  // s = 1000/((4095/3.3)*(1/11)) = 8.8623046875

  // Avoid needing floating point math, so output in mV
  return (adc_get(ADCCHAN_VOLTAGE) * 1000U) / 19893; //REDEBUG for red panda, output of the divider
}
