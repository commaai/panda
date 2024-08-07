
const uint8_t sine[360] = { 127U, 129U, 131U, 133U, 135U, 138U, 140U, 142U, 144U, 146U, 149U, 151U, 153U, 155U, 157U, 159U, 161U, 164U, 166U, 168U, 170U, 172U, 174U, 176U, 178U, 180U, 182U, 184U, 186U, 188U, 190U, 192U, 194U, 196U, 197U, 199U, 201U, 203U, 205U, 206U, 208U, 210U, 211U, 213U, 215U, 216U, 218U, 219U, 221U, 222U, 224U, 225U, 227U, 228U, 229U, 230U, 232U, 233U, 234U, 235U, 236U, 238U, 239U, 240U, 241U, 242U, 242U, 243U, 244U, 245U, 246U, 247U, 247U, 248U, 249U, 249U, 250U, 250U, 251U, 251U, 252U, 252U, 252U, 253U, 253U, 253U, 253U, 253U, 253U, 253U, 253U, 253U, 253U, 253U, 253U, 253U, 253U, 253U, 252U, 252U, 252U, 251U, 251U, 250U, 250U, 249U, 249U, 248U, 247U, 247U, 246U, 245U, 244U, 243U, 243U, 242U, 241U, 240U, 239U, 238U, 237U, 235U, 234U, 233U, 232U, 231U, 229U, 228U, 227U, 225U, 224U, 222U, 221U, 219U, 218U, 216U, 215U, 213U, 212U, 210U, 208U, 207U, 205U, 203U, 201U, 199U, 198U, 196U, 194U, 192U, 190U, 188U, 186U, 184U, 182U, 180U, 178U, 176U, 174U, 172U, 170U, 168U, 166U, 164U, 162U, 160U, 157U, 155U, 153U, 151U, 149U, 147U, 144U, 142U, 140U, 138U, 136U, 133U, 131U, 129U, 127U, 124U, 122U, 120U, 118U, 116U, 113U, 111U, 109U, 107U, 105U, 102U, 100U, 98U, 96U, 94U, 92U, 90U, 87U, 85U, 83U, 81U, 79U, 77U, 75U, 73U, 71U, 69U, 67U, 65U, 63U, 61U, 59U, 58U, 56U, 54U, 52U, 50U, 49U, 47U, 45U, 43U, 42U, 40U, 38U, 37U, 35U, 34U, 32U, 31U, 29U, 28U, 27U, 25U, 24U, 23U, 21U, 20U, 19U, 18U, 17U, 16U, 14U, 13U, 12U, 12U, 11U, 10U, 9U, 8U, 7U, 7U, 6U, 5U, 4U, 4U, 3U, 3U, 2U, 2U, 1U, 1U, 1U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 1U, 1U, 1U, 2U, 2U, 3U, 3U, 4U, 4U, 5U, 6U, 6U, 7U, 8U, 9U, 9U, 10U, 11U, 12U, 13U, 14U, 15U, 16U, 17U, 19U, 20U, 21U, 22U, 24U, 25U, 26U, 28U, 29U, 30U, 32U, 33U, 35U, 36U, 38U, 40U, 41U, 43U, 45U, 46U, 48U, 50U, 52U, 53U, 55U, 57U, 59U, 61U, 63U, 65U, 67U, 69U, 70U, 72U, 74U, 77U, 79U, 81U, 83U, 85U, 87U, 89U, 91U, 93U, 95U, 98U, 100U, 102U, 104U, 106U, 108U, 111U, 113U, 115U, 117U, 119U, 122U, 124U };

// void sai1_init(void) {
//   register_set(&SAI1->GCR, 0U, SAI_GCR_SYNCIN_Msk | SAI_GCR_SYNCOUT_Msk);

//   register_set(&SAI1_Block_A->CR1, (26U << SAI_xCR1_MCKDIV_Pos) | SAI_xCR1_NODIV | SAI_xCR1_DMAEN | (0b00 << SAI_xCR1_SYNCEN_Pos) | (0b111 << SAI_xCR1_DS_Pos) | (0b01 << SAI_xCR1_MODE_Pos), 0x0FFB3FEFU);
//   register_set(&SAI1_Block_A->CR2, 0U, 0xFFFFU);
//   register_set(&SAI1_Block_A->FRCR, (31U << SAI_xFRCR_FRL_Pos) | (0b1 << SAI_xFRCR_FSPOL_Pos), 0x7FFFFU);
//   register_set(&SAI1_Block_A->SLOTR, (0b11 << SAI_xSLOTR_SLOTEN_Pos) | (1U << SAI_xSLOTR_NBSLOT_Pos) | (0b11 << SAI_xSLOTR_SLOTSZ_Pos), 0xFFFF0FDFU); // NBSLOT definition is vague

//   register_set(&SAI1->PDMCR, (0b1 << SAI_PDMCR_CKEN2_Pos) | (0b01 << SAI_PDMCR_MICNBR_Pos) | (0b1 << SAI_PDMCR_PDMEN_Pos), 0x331);
//   register_set_bits(&SAI1_Block_A->CR1, SAI_xCR1_SAIEN);
// }

void sai4_init(void) {
  register_set(&SAI4->GCR, 0U, SAI_GCR_SYNCIN_Msk | SAI_GCR_SYNCOUT_Msk);

  // mono audio in
  register_set(&SAI4_Block_A->CR1, SAI_xCR1_MONO | SAI_xCR1_DMAEN | (0b01 << SAI_xCR1_SYNCEN_Pos) | (0b100 << SAI_xCR1_DS_Pos) | (0b11 << SAI_xCR1_MODE_Pos), 0x0FFB3FEFU);
  register_set(&SAI4_Block_A->CR2, 0U, 0xFFFBU); // TODO: mute detection
  register_set(&SAI4_Block_A->FRCR, (15U << SAI_xFRCR_FRL_Pos), 0x7FFFFU); // is this correct?
  register_set(&SAI4_Block_A->SLOTR, (0b11 << SAI_xSLOTR_SLOTEN_Pos) | (1U << SAI_xSLOTR_NBSLOT_Pos) | (0b01 << SAI_xSLOTR_SLOTSZ_Pos), 0xFFFF0FDFU); // NBSLOT definition is vague

  // mono mic out
  // register_set(&SAI4_Block_A->CR1, (26U << SAI_xCR1_MCKDIV_Pos) | SAI_xCR1_NODIV | SAI_xCR1_DMAEN | (0b00 << SAI_xCR1_SYNCEN_Pos) | (0b111 << SAI_xCR1_DS_Pos) | (0b01 << SAI_xCR1_MODE_Pos), 0x0FFB3FEFU);
  // register_set(&SAI4_Block_A->CR2, 0U, 0xFFFFU);
  // register_set(&SAI4_Block_A->FRCR, (31U << SAI_xFRCR_FRL_Pos) | (0b1 << SAI_xFRCR_FSPOL_Pos), 0x7FFFFU);
  // register_set(&SAI4_Block_A->SLOTR, (0b11 << SAI_xSLOTR_SLOTEN_Pos) | (1U << SAI_xSLOTR_NBSLOT_Pos) | (0b11 << SAI_xSLOTR_SLOTSZ_Pos), 0xFFFF0FDFU); // NBSLOT definition is vague

  // register_set(&SAI4->PDMCR, (0b1 << SAI_PDMCR_CKEN2_Pos) | (0b01 << SAI_PDMCR_MICNBR_Pos) | (0b1 << SAI_PDMCR_PDMEN_Pos), 0x331);

  register_set_bits(&SAI4_Block_A->CR1, SAI_xCR1_SAIEN);
  // register_set_bits(&SAI4_Block_B->CR1, SAI_xCR1_SAIEN);
}

void sound_init(void) {
  // Init DAC
  // register_set(&DAC1->MCR, 0U, 0xFFFFFFFFU);
  // register_set(&DAC1->CR, DAC_CR_TEN1 | (6U << DAC_CR_TSEL1_Pos) | DAC_CR_DMAEN1, 0xFFFFFFFFU);
  // register_set_bits(&DAC1->CR, DAC_CR_EN1);

  // Setup DMAMUX (DAC_CH1_DMA as input)
  // register_set(&DMAMUX1_Channel1->CCR, 67U, DMAMUX_CxCR_DMAREQ_ID_Msk);

  // Setup DMA
  // register_set(&DMA1_Stream1->M0AR, (uint32_t) sine, 0xFFFFFFFFU);
  // register_set(&DMA1_Stream1->PAR, (uint32_t) &(DAC1->DHR8R1), 0xFFFFFFFFU);
  // DMA1_Stream1->NDTR = sizeof(sine);
  // register_set(&DMA1_Stream1->FCR, 0U, 0x00000083U);
  // DMA1_Stream1->CR = (0b11 << DMA_SxCR_PL_Pos);
  // DMA1_Stream1->CR |= DMA_SxCR_MINC | DMA_SxCR_CIRC | (1 << DMA_SxCR_DIR_Pos);
  // register_set_bits(&DMA1_Stream1->CR, DMA_SxCR_EN);

  // Init trigger timer
  // register_set(&TIM7->PSC, 0U, 0xFFFFU);
  // register_set(&TIM7->ARR, 200U, 0xFFFFU);
  // register_set(&TIM7->CR2, (0b10 << TIM_CR2_MMS_Pos), TIM_CR2_MMS_Msk);
  // register_set(&TIM7->CR1, TIM_CR1_ARPE | TIM_CR1_URS, 0x088EU);
  // TIM7->SR = 0U;
  // TIM7->CR1 |= TIM_CR1_CEN;

  // sai1_init();
  sai4_init();
}