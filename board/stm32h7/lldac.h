
// TODO: generalize for more channels. We only need DAC1 CH1 for now

void dac_init(void) {
  // Initialize DAC1_CH1
  DAC1->CR = 0U;
  DAC1->MCR = 0U;
  DAC1->CR = DAC_CR_EN1;
}

// Set channel 1 value, in mV
void dac_set(uint32_t value) {
  DAC1->DHR8R1 = MAX(MIN(value * (1U << 8U) / 3300U, (1U << 8U)), 0U);
  //DAC1->SWTRIGR |= DAC_SWTRIGR_SWTRIG1;
}