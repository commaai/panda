bool fake_siren_enabled = false;

// 1Vpp sine wave
uint8_t fake_siren_lut[360] = { 38U, 39U, 40U, 40U, 41U, 42U, 43U, 43U, 44U, 45U, 45U, 46U, 46U, 47U, 48U, 48U, 49U, 50U, 50U, 51U, 52U, 52U, 53U, 54U, 54U, 55U, 55U, 56U, 57U, 57U, 58U, 58U, 59U, 59U, 60U, 61U, 61U, 62U, 62U, 63U, 63U, 64U, 64U, 65U, 65U, 66U, 66U, 67U, 67U, 68U, 68U, 68U, 69U, 69U, 70U, 70U, 70U, 71U, 71U, 71U, 72U, 72U, 72U, 73U, 73U, 73U, 74U, 74U, 74U, 74U, 75U, 75U, 75U, 75U, 75U, 76U, 76U, 76U, 76U, 76U, 76U, 76U, 77U, 77U, 77U, 77U, 77U, 77U, 77U, 77U, 77U, 77U, 77U, 77U, 77U, 77U, 77U, 77U, 77U, 76U, 76U, 76U, 76U, 76U, 76U, 76U, 75U, 75U, 75U, 75U, 75U, 74U, 74U, 74U, 74U, 73U, 73U, 73U, 72U, 72U, 72U, 71U, 71U, 71U, 70U, 70U, 70U, 69U, 69U, 68U, 68U, 68U, 67U, 67U, 66U, 66U, 65U, 65U, 64U, 64U, 63U, 63U, 62U, 62U, 61U, 61U, 60U, 59U, 59U, 58U, 58U, 57U, 57U, 56U, 55U, 55U, 54U, 54U, 53U, 52U, 52U, 51U, 50U, 50U, 49U, 48U, 48U, 47U, 46U, 46U, 45U, 45U, 44U, 43U, 43U, 42U, 41U, 40U, 40U, 39U, 38U, 38U, 37U, 36U, 36U, 35U, 34U, 34U, 33U, 32U, 32U, 31U, 30U, 30U, 29U, 29U, 28U, 27U, 27U, 26U, 25U, 25U, 24U, 23U, 23U, 22U, 22U, 21U, 20U, 20U, 19U, 19U, 18U, 18U, 17U, 16U, 16U, 15U, 15U, 14U, 14U, 13U, 13U, 12U, 12U, 11U, 11U, 10U, 10U, 9U, 9U, 9U, 8U, 8U, 7U, 7U, 7U, 6U, 6U, 5U, 5U, 5U, 5U, 4U, 4U, 4U, 3U, 3U, 3U, 3U, 2U, 2U, 2U, 2U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 2U, 2U, 2U, 2U, 3U, 3U, 3U, 3U, 4U, 4U, 4U, 5U, 5U, 5U, 5U, 6U, 6U, 7U, 7U, 7U, 8U, 8U, 9U, 9U, 9U, 10U, 10U, 11U, 11U, 12U, 12U, 13U, 13U, 14U, 14U, 15U, 15U, 16U, 16U, 17U, 18U, 18U, 19U, 19U, 20U, 20U, 21U, 22U, 22U, 23U, 23U, 24U, 25U, 25U, 26U, 27U, 27U, 28U, 29U, 29U, 30U, 30U, 31U, 32U, 32U, 33U, 34U, 34U, 35U, 36U, 36U, 37U, 38U };

void fake_siren_set(bool enabled) {
  fake_siren_enabled = enabled;

  if (enabled) {
    register_set_bits(&DMA2_Stream1->CR, DMA_SxCR_EN);
  } else {
    register_clear_bits(&DMA2_Stream1->CR, DMA_SxCR_EN);
  }
}

void fake_siren_init(void) {
  // Init DAC
  dac_init(DAC1, 1, true);

  // Setup DMA (bytes, increment memory pointer, circular, M->P)
  register_set(&DMA2_Stream1->CR, 0U, DMA_SxCR_MSIZE);
  register_set(&DMA2_Stream1->CR, 0U, DMA_SxCR_PSIZE);
  register_set_bits(&DMA2_Stream1->CR, DMA_SxCR_MINC);
  register_set_bits(&DMA2_Stream1->CR, DMA_SxCR_CIRC);
  register_set(&DMA2_Stream1->CR, (0b01 << DMA_SxCR_DIR_Pos), DMA_SxCR_DIR);

  // Number of bytes
  register_set(&DMA2_Stream1->NDTR, sizeof(fake_siren_lut), DMA_SxNDT);

  // Pointers
  register_set(&DMA2_Stream1->PAR, (uint32_t) &DAC1->DHR8R1, DMA_SxPAR_PA);
  register_set(&DMA2_Stream1->M0AR, (uint32_t) fake_siren_lut, DMA_SxM0AR_M0A);
}