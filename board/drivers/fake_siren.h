bool fake_siren_enabled = false;

// 1Vpp sine wave with 1V offset
const uint8_t fake_siren_lut[360] = { 134U, 135U, 137U, 138U, 139U, 140U, 141U, 143U, 144U, 145U, 146U, 148U, 149U, 150U, 151U, 152U, 154U, 155U, 156U, 157U, 158U, 159U, 160U, 162U, 163U, 164U, 165U, 166U, 167U, 168U, 169U, 170U, 171U, 172U, 174U, 175U, 176U, 177U, 177U, 178U, 179U, 180U, 181U, 182U, 183U, 184U, 185U, 186U, 186U, 187U, 188U, 189U, 190U, 190U, 191U, 192U, 193U, 193U, 194U, 195U, 195U, 196U, 196U, 197U, 197U, 198U, 199U, 199U, 199U, 200U, 200U, 201U, 201U, 202U, 202U, 202U, 203U, 203U, 203U, 203U, 204U, 204U, 204U, 204U, 204U, 204U, 204U, 205U, 205U, 205U, 205U, 205U, 205U, 205U, 204U, 204U, 204U, 204U, 204U, 204U, 204U, 203U, 203U, 203U, 203U, 202U, 202U, 202U, 201U, 201U, 200U, 200U, 199U, 199U, 199U, 198U, 197U, 197U, 196U, 196U, 195U, 195U, 194U, 193U, 193U, 192U, 191U, 190U, 190U, 189U, 188U, 187U, 186U, 186U, 185U, 184U, 183U, 182U, 181U, 180U, 179U, 178U, 177U, 177U, 176U, 175U, 174U, 172U, 171U, 170U, 169U, 168U, 167U, 166U, 165U, 164U, 163U, 162U, 160U, 159U, 158U, 157U, 156U, 155U, 154U, 152U, 151U, 150U, 149U, 148U, 146U, 145U, 144U, 143U, 141U, 140U, 139U, 138U, 137U, 135U, 134U, 133U, 132U, 130U, 129U, 128U, 127U, 125U, 124U, 123U, 122U, 121U, 119U, 118U, 117U, 116U, 115U, 113U, 112U, 111U, 110U, 109U, 108U, 106U, 105U, 104U, 103U, 102U, 101U, 100U, 99U, 98U, 97U, 96U, 95U, 94U, 93U, 92U, 91U, 90U, 89U, 88U, 87U, 86U, 85U, 84U, 83U, 82U, 82U, 81U, 80U, 79U, 78U, 78U, 77U, 76U, 76U, 75U, 74U, 74U, 73U, 72U, 72U, 71U, 71U, 70U, 70U, 69U, 69U, 68U, 68U, 67U, 67U, 67U, 66U, 66U, 66U, 65U, 65U, 65U, 65U, 64U, 64U, 64U, 64U, 64U, 64U, 64U, 64U, 64U, 63U, 64U, 64U, 64U, 64U, 64U, 64U, 64U, 64U, 64U, 65U, 65U, 65U, 65U, 66U, 66U, 66U, 67U, 67U, 67U, 68U, 68U, 69U, 69U, 70U, 70U, 71U, 71U, 72U, 72U, 73U, 74U, 74U, 75U, 76U, 76U, 77U, 78U, 78U, 79U, 80U, 81U, 82U, 82U, 83U, 84U, 85U, 86U, 87U, 88U, 89U, 90U, 91U, 92U, 93U, 94U, 95U, 96U, 97U, 98U, 99U, 100U, 101U, 102U, 103U, 104U, 105U, 106U, 108U, 109U, 110U, 111U, 112U, 113U, 115U, 116U, 117U, 118U, 119U, 121U, 122U, 123U, 124U, 125U, 127U, 128U, 129U, 130U, 132U, 133U };

void fake_siren_set(bool enabled) {
  fake_siren_enabled = enabled;

  if (enabled) {
    register_set_bits(&DMA1_Stream1->CR, DMA_SxCR_EN);
  } else {
    register_clear_bits(&DMA1_Stream1->CR, DMA_SxCR_EN);
  }
}

#define CODEC_I2C_ADDR 0x20
void set_codec_register(uint8_t reg, uint8_t value) {
  // Setup transfer and send START + addr
  while(true) {
    register_clear_bits(&I2C5->CR2, I2C_CR2_ADD10);
    I2C5->CR2 = (CODEC_I2C_ADDR & I2C_CR2_SADD_Msk);
    register_clear_bits(&I2C5->CR2, I2C_CR2_RD_WRN);
    register_set_bits(&I2C5->CR2, I2C_CR2_AUTOEND);
    I2C5->CR2 |= (2 << I2C_CR2_NBYTES_Pos);

    I2C5->CR2 |= I2C_CR2_START;
    while((I2C5->CR2 & I2C_CR2_START) != 0U);

    // check if we lost arbitration
    if ((I2C5->ISR & I2C_ISR_ARLO) != 0U) {
      register_set_bits(&I2C5->ICR, I2C_ICR_ARLOCF);
    } else {
      break;
    }
  }

  // Send data
  while((I2C5->ISR & I2C_ISR_TXIS) == 0U);
  I2C5->TXDR = reg;
  while((I2C5->ISR & I2C_ISR_TXIS) == 0U);
  I2C5->TXDR = value;

  I2C5->CR2 |= I2C_CR2_STOP;
}

void fake_siren_init(void) {
  // TODO: convert to register_ functions

  // Init DAC
  DAC1->MCR = 0U;
  DAC1->CR = DAC_CR_TEN1 | (6U << DAC_CR_TSEL1_Pos) | DAC_CR_DMAEN1;
  DAC1->CR |= DAC_CR_EN1;

  // Setup DMAMUX
  DMAMUX1_Channel1->CCR = (67U << DMAMUX_CxCR_DMAREQ_ID_Pos); // DAC_CH1_DMA as input

  // Setup DMA
  DMA1_Stream1->M0AR = (uint32_t) fake_siren_lut;
  DMA1_Stream1->PAR = (uint32_t) &(DAC1->DHR8R1);
  DMA1_Stream1->NDTR = sizeof(fake_siren_lut);
  DMA1_Stream1->FCR = 0U;
  DMA1_Stream1->CR = (0b11 << DMA_SxCR_PL_Pos);
  DMA1_Stream1->CR |= DMA_SxCR_MINC | DMA_SxCR_CIRC | (1 << DMA_SxCR_DIR_Pos);

  // Init trigger timer (around 2.5kHz)
  TIM7->PSC = 0;
  TIM7->ARR = 133;
  TIM7->CR2 = (2 << TIM_CR2_MMS_Pos);
  TIM7->CR1 = TIM_CR1_ARPE | TIM_CR1_URS;
  TIM7->DIER = TIM_DIER_UIE;
  TIM7->SR = 0U;
  TIM7->CR1 |= TIM_CR1_CEN;

  // Enable the I2C to the codec
  I2C5->TIMINGR = 0x107075B0;
  I2C5->CR1 = I2C_CR1_PE;

  set_codec_register(0x4C, 0x88);
}