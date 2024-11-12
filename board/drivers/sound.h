
const uint8_t num_bits[256] = {0U, 1U, 1U, 2U, 1U, 2U, 2U, 3U, 1U, 2U, 2U, 3U, 2U, 3U, 3U, 4U, 1U, 2U, 2U, 3U, 2U, 3U, 3U, 4U, 2U, 3U, 3U, 4U, 3U, 4U, 4U, 5U, 1U, 2U, 2U, 3U, 2U, 3U, 3U, 4U, 2U, 3U, 3U, 4U, 3U, 4U, 4U, 5U, 2U, 3U, 3U, 4U, 3U, 4U, 4U, 5U, 3U, 4U, 4U, 5U, 4U, 5U, 5U, 6U, 1U, 2U, 2U, 3U, 2U, 3U, 3U, 4U, 2U, 3U, 3U, 4U, 3U, 4U, 4U, 5U, 2U, 3U, 3U, 4U, 3U, 4U, 4U, 5U, 3U, 4U, 4U, 5U, 4U, 5U, 5U, 6U, 2U, 3U, 3U, 4U, 3U, 4U, 4U, 5U, 3U, 4U, 4U, 5U, 4U, 5U, 5U, 6U, 3U, 4U, 4U, 5U, 4U, 5U, 5U, 6U, 4U, 5U, 5U, 6U, 5U, 6U, 6U, 7U, 1U, 2U, 2U, 3U, 2U, 3U, 3U, 4U, 2U, 3U, 3U, 4U, 3U, 4U, 4U, 5U, 2U, 3U, 3U, 4U, 3U, 4U, 4U, 5U, 3U, 4U, 4U, 5U, 4U, 5U, 5U, 6U, 2U, 3U, 3U, 4U, 3U, 4U, 4U, 5U, 3U, 4U, 4U, 5U, 4U, 5U, 5U, 6U, 3U, 4U, 4U, 5U, 4U, 5U, 5U, 6U, 4U, 5U, 5U, 6U, 5U, 6U, 6U, 7U, 2U, 3U, 3U, 4U, 3U, 4U, 4U, 5U, 3U, 4U, 4U, 5U, 4U, 5U, 5U, 6U, 3U, 4U, 4U, 5U, 4U, 5U, 5U, 6U, 4U, 5U, 5U, 6U, 5U, 6U, 6U, 7U, 3U, 4U, 4U, 5U, 4U, 5U, 5U, 6U, 4U, 5U, 5U, 6U, 5U, 6U, 6U, 7U, 4U, 5U, 5U, 6U, 5U, 6U, 6U, 7U, 5U, 6U, 6U, 7U, 6U, 7U, 7U, 8U};

// TODO: also double-buffer TX
#define RX_BUF_SIZE 2000U
#define TX_BUF_SIZE (RX_BUF_SIZE/2U)

#define PDM_DECIMATION 13U
#define MIC_RX_BUF_SIZE 1024U
#define MIC_TX_BUF_SIZE (MIC_RX_BUF_SIZE/PDM_DECIMATION)

__attribute__((section(".sram4"))) uint16_t rx_buf[2][RX_BUF_SIZE];
__attribute__((section(".sram4"))) uint16_t tx_buf[TX_BUF_SIZE];
__attribute__((section(".sram4"))) uint16_t mic_rx_buf[2][MIC_RX_BUF_SIZE];
__attribute__((section(".sram4"))) uint16_t mic_tx_buf[MIC_TX_BUF_SIZE];

// Playback processing
void BDMA_Channel0_IRQ_Handler(void) {
  BDMA->IFCR |= BDMA_IFCR_CGIF0; // clear flag

  // process samples (shift to 12b and bias to be unsigned)
  // since we are playing mono and receiving stereo, we take every other sample
  uint8_t buf_idx = ((BDMA_Channel0->CCR & BDMA_CCR_CT) >> BDMA_CCR_CT_Pos) == 1U ? 0U : 1U;
  for (uint16_t i=0U; i < RX_BUF_SIZE; i += 2) {
    tx_buf[i/2U] = ((((int32_t) rx_buf[buf_idx][i]) + (1U << 14)) >> 3);
  }

  DMA1->LIFCR |= 0xF40;
  DMA1_Stream1->CR &= ~DMA_SxCR_EN;
  register_set(&DMA1_Stream1->M0AR, (uint32_t) tx_buf, 0xFFFFFFFFU);
  DMA1_Stream1->NDTR = TX_BUF_SIZE;
  DMA1_Stream1->CR |= DMA_SxCR_EN;
}

// Recording processing
void DMA1_Stream0_IRQ_Handler(void) {
  DMA1->LIFCR |= 0x7D; // clear flags

  // process samples (convert MIC2L 8b PDM -> 16b PCM)
  uint8_t buf_idx = ((DMA1_Stream0->CR & DMA_SxCR_CT) >> DMA_SxCR_CT_Pos) == 1U ? 0U : 1U;
  for (uint16_t i=0U; i < MIC_TX_BUF_SIZE; i++) {
    uint16_t sum = 0U;
    for (uint8_t j=0U; j < PDM_DECIMATION; j++) {
      sum += num_bits[(mic_rx_buf[buf_idx][(i * PDM_DECIMATION) + j] >> 8U) & 0xFF];
    }
    mic_tx_buf[i] = ((int16_t) sum - (4U * PDM_DECIMATION)) * ((1U << 16) / (8U * PDM_DECIMATION));
  }

  BDMA->IFCR |= BDMA_IFCR_CGIF1;
  BDMA_Channel1->CCR &= ~BDMA_CCR_EN;
  register_set(&BDMA_Channel1->CM0AR, (uint32_t) mic_tx_buf, 0xFFFFFFFFU);
  BDMA_Channel1->CNDTR = MIC_TX_BUF_SIZE;
  BDMA_Channel1->CCR |= BDMA_CCR_EN;
}

void sound_init(void) {
  REGISTER_INTERRUPT(BDMA_Channel0_IRQn, BDMA_Channel0_IRQ_Handler, 64U, FAULT_INTERRUPT_RATE_SOUND_DMA)
  REGISTER_INTERRUPT(DMA1_Stream0_IRQn, DMA1_Stream0_IRQ_Handler, 128U, FAULT_INTERRUPT_RATE_SOUND_DMA)

  // Init DAC
  register_set(&DAC1->MCR, 0U, 0xFFFFFFFFU);
  register_set(&DAC1->CR, DAC_CR_TEN1 | (6U << DAC_CR_TSEL1_Pos) | DAC_CR_DMAEN1, 0xFFFFFFFFU);
  register_set_bits(&DAC1->CR, DAC_CR_EN1);

  // Setup DMAMUX (DAC_CH1_DMA as input)
  register_set(&DMAMUX1_Channel1->CCR, 67U, DMAMUX_CxCR_DMAREQ_ID_Msk);

  // Setup DMA
  register_set(&DMA1_Stream1->PAR, (uint32_t) &(DAC1->DHR12R1), 0xFFFFFFFFU);
  register_set(&DMA1_Stream1->FCR, 0U, 0x00000083U);
  DMA1_Stream1->CR = (0b11 << DMA_SxCR_PL_Pos) | (0b01 << DMA_SxCR_MSIZE_Pos) | (0b01 << DMA_SxCR_PSIZE_Pos) | DMA_SxCR_MINC | (1 << DMA_SxCR_DIR_Pos);

  // Init trigger timer (48kHz)
  register_set(&TIM7->PSC, 0U, 0xFFFFU);
  register_set(&TIM7->ARR, 2494U, 0xFFFFU);
  register_set(&TIM7->CR2, (0b10 << TIM_CR2_MMS_Pos), TIM_CR2_MMS_Msk);
  register_set(&TIM7->CR1, TIM_CR1_ARPE | TIM_CR1_URS, 0x088EU);
  TIM7->SR = 0U;
  TIM7->CR1 |= TIM_CR1_CEN;

  // sync both SAIs
  register_set(&SAI4->GCR, (0b10 << SAI_GCR_SYNCOUT_Pos), SAI_GCR_SYNCIN_Msk | SAI_GCR_SYNCOUT_Msk);
  register_set(&SAI1->GCR, (3U << SAI_GCR_SYNCIN_Pos), SAI_GCR_SYNCIN_Msk | SAI_GCR_SYNCOUT_Msk);

  // stereo audio in
  register_set(&SAI4_Block_B->CR1, SAI_xCR1_DMAEN | (0b00 << SAI_xCR1_SYNCEN_Pos) | (0b100 << SAI_xCR1_DS_Pos) | (0b11 << SAI_xCR1_MODE_Pos), 0x0FFB3FEFU);
  register_set(&SAI4_Block_B->CR2, (0b001 << SAI_xCR2_FTH_Pos), 0xFFFBU); // TODO: mute detection
  register_set(&SAI4_Block_B->FRCR, (31U << SAI_xFRCR_FRL_Pos), 0x7FFFFU);
  register_set(&SAI4_Block_B->SLOTR, (0b11 << SAI_xSLOTR_SLOTEN_Pos) | (1U << SAI_xSLOTR_NBSLOT_Pos) | (0b01 << SAI_xSLOTR_SLOTSZ_Pos), 0xFFFF0FDFU); // NBSLOT definition is vague

  // mono mic in
  register_set(&SAI1->PDMCR, (0b1 << SAI_PDMCR_CKEN2_Pos) | (0b01 << SAI_PDMCR_MICNBR_Pos) | (0b1 << SAI_PDMCR_PDMEN_Pos), 0x331);
  register_set(&SAI1_Block_A->CR1, SAI_xCR1_DMAEN | (2U << SAI_xCR1_MCKDIV_Pos) | SAI_xCR1_NODIV | (0b00 << SAI_xCR1_SYNCEN_Pos) | (0b111 << SAI_xCR1_DS_Pos) | (0b01 << SAI_xCR1_MODE_Pos), 0x0FFB3FEFU);
  register_set(&SAI1_Block_A->CR2, 0U, 0xFFFBU);
  register_set(&SAI1_Block_A->FRCR, SAI_xFRCR_FSPOL | (31U << SAI_xFRCR_FRL_Pos), 0x7FFFFU);
  register_set(&SAI1_Block_A->SLOTR, (0b01 << SAI_xSLOTR_SLOTEN_Pos) | (0U << SAI_xSLOTR_NBSLOT_Pos) | (0b00 << SAI_xSLOTR_SLOTSZ_Pos), 0xFFFF0FDFU); // NBSLOT definition is vague

  // mono mic out (slave transmitter)
  register_set(&SAI4_Block_A->CR1, SAI_xCR1_DMAEN | (0b01 << SAI_xCR1_SYNCEN_Pos) | (0b100 << SAI_xCR1_DS_Pos) | (0b10 << SAI_xCR1_MODE_Pos), 0x0FFB3FEFU);
  register_set(&SAI4_Block_A->CR2, 0U, 0xFFFBU);
  register_set(&SAI4_Block_A->FRCR, (31U << SAI_xFRCR_FRL_Pos), 0x7FFFFU);
  register_set(&SAI4_Block_A->SLOTR, (0b11 << SAI_xSLOTR_SLOTEN_Pos) | (1U << SAI_xSLOTR_NBSLOT_Pos) | (0b01 << SAI_xSLOTR_SLOTSZ_Pos), 0xFFFF0FDFU); // NBSLOT definition is vague

  // init sound DMA (SAI4_B -> memory, double buffers)
  register_set(&BDMA_Channel0->CPAR, (uint32_t) &(SAI4_Block_B->DR), 0xFFFFFFFFU);
  register_set(&BDMA_Channel0->CM0AR, (uint32_t) rx_buf[0], 0xFFFFFFFFU);
  register_set(&BDMA_Channel0->CM1AR, (uint32_t) rx_buf[1], 0xFFFFFFFFU);
  BDMA_Channel0->CNDTR = RX_BUF_SIZE;
  register_set(&BDMA_Channel0->CCR, BDMA_CCR_DBM | (0b01 << BDMA_CCR_MSIZE_Pos) | (0b01 << BDMA_CCR_PSIZE_Pos) | BDMA_CCR_MINC | BDMA_CCR_CIRC | BDMA_CCR_TCIE, 0xFFFFU);
  register_set(&DMAMUX2_Channel0->CCR, 16U, DMAMUX_CxCR_DMAREQ_ID_Msk); // SAI4_B_DMA
  register_set_bits(&BDMA_Channel0->CCR, BDMA_CCR_EN);

  // init mic DMA 1 (SAI1_A -> memory)
  register_set(&DMA1_Stream0->PAR, (uint32_t) &(SAI1_Block_A->DR), 0xFFFFFFFFU);
  register_set(&DMA1_Stream0->M0AR, (uint32_t) mic_rx_buf[0], 0xFFFFFFFFU);
  register_set(&DMA1_Stream0->M1AR, (uint32_t) mic_rx_buf[1], 0xFFFFFFFFU);
  DMA1_Stream0->NDTR = MIC_RX_BUF_SIZE;
  register_clear_bits(&DMA1_Stream0->CR, DMA_SxCR_EN);
  register_set(&DMA1_Stream0->CR, DMA_SxCR_DBM | (0b01 << DMA_SxCR_MSIZE_Pos) | (0b01 << DMA_SxCR_PSIZE_Pos) | DMA_SxCR_MINC | DMA_SxCR_CIRC | DMA_SxCR_TCIE, 0x1F7FFFFU);
  register_set(&DMAMUX1_Channel0->CCR, 87U, DMAMUX_CxCR_DMAREQ_ID_Msk); // SAI1_A_DMA
  register_set_bits(&DMA1_Stream0->CR, DMA_SxCR_EN);

  // init mic DMA 2 (memory -> SAI4_A)
  register_set(&BDMA_Channel1->CPAR, (uint32_t) &(SAI4_Block_A->DR), 0xFFFFFFFFU);
  register_set(&BDMA_Channel1->CCR, (0b01 << BDMA_CCR_MSIZE_Pos) | (0b01 << BDMA_CCR_PSIZE_Pos) | BDMA_CCR_MINC | (0b1 << BDMA_CCR_DIR_Pos), 0xFFFEU);
  register_set(&DMAMUX2_Channel1->CCR, 15U, DMAMUX_CxCR_DMAREQ_ID_Msk); // SAI4_A_DMA

  // enable all initted blocks
  register_set_bits(&SAI1_Block_A->CR1, SAI_xCR1_SAIEN);
  register_set_bits(&SAI4_Block_A->CR1, SAI_xCR1_SAIEN);
  register_set_bits(&SAI4_Block_B->CR1, SAI_xCR1_SAIEN);
  NVIC_EnableIRQ(BDMA_Channel0_IRQn);
  NVIC_EnableIRQ(DMA1_Stream0_IRQn);
}