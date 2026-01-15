
void llspi_dump_state(void){
  print("    STATE: "); puth(spi_state); print("\n");
  print("    SPI4 CR1: "); puth(SPI4->CR1);
  print(" CR2: "); puth(SPI4->CR2);
  print(" SR: "); puth(SPI4->SR);
  print(" IER: "); puth(SPI4->IER);
  print(" CFG1: "); puth(SPI4->CFG1);
  print(" CFG2: "); puth(SPI4->CFG2);
  print("\n");
  // print("    DMA2 Stream2 CR: "); puth(DMA2_Stream2->CR);
  // print(" NDTR: "); puth(DMA2_Stream2->NDTR); print("\n");
  print("    DMA2 Stream3 CR: "); puth(DMA2_Stream3->CR);
  print(" NDTR: "); puth(DMA2_Stream3->NDTR); 
  print("\n\n");
}

static void llspi_disable(void) {
  // disable DMA + SPI  
  DMA2_Stream2->CR &= ~DMA_SxCR_EN;
  DMA2_Stream3->CR &= ~DMA_SxCR_EN;
  while((DMA2_Stream2->CR & DMA_SxCR_EN) != 0U);
  while((DMA2_Stream3->CR & DMA_SxCR_EN) != 0U);
  register_clear_bits(&(SPI4->CR1), SPI_CR1_SPE);
  register_clear_bits(&(SPI4->CFG1), SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN);
}

// master -> panda DMA start
void llspi_mosi_dma(uint8_t *addr, int len) {
  // drain the bus
  while ((SPI4->SR & SPI_SR_RXP) != 0U) {
    volatile uint8_t dat = SPI4->RXDR;
    (void)dat;
  }

  // clear all pending
  SPI4->IFCR |= (0x1FFU << 3U);
  register_clear_bits(&(SPI4->IER), SPI_IER_EOTIE);

  // simplex receive mode
  register_set(&(SPI4->CFG2), (0b10 << SPI_CFG2_COMM_Pos), SPI_CFG2_COMM_Msk);

  // setup destination and length
  register_set(&(DMA2_Stream2->M0AR), (uint32_t)addr, 0xFFFFFFFFU);
  DMA2_Stream2->NDTR = len;

  // enable DMA + SPI
  DMA2_Stream2->CR |= DMA_SxCR_EN;
  register_set_bits(&(SPI4->CFG1), SPI_CFG1_RXDMAEN);
  register_set_bits(&(SPI4->CR1), SPI_CR1_SPE);
}

// panda -> master DMA start
void llspi_miso_dma(uint8_t *addr, int len) {
  // setup source and length
  register_set(&(DMA2_Stream3->M0AR), (uint32_t)addr, 0xFFFFFFFFU);
  DMA2_Stream3->NDTR = len;

  // clear under-run while we were reading
  SPI4->IFCR |= (0x1FFU << 3U);

  // simplex transmit mode
  register_set(&(SPI4->CFG2), (0b01 << SPI_CFG2_COMM_Pos), SPI_CFG2_COMM_Msk);

  // setup interrupt on TXC
  register_set_bits(&(SPI4->IER), SPI_IER_EOTIE);

  // enable DMA + SPI
  DMA2_Stream3->CR |= DMA_SxCR_EN;
  register_set_bits(&(SPI4->CFG1), SPI_CFG1_TXDMAEN);  
  register_set_bits(&(SPI4->CR1), SPI_CR1_SPE);
}

static bool spi_tx_dma_done = false;
// master -> panda DMA finished
static void DMA2_Stream2_IRQ_Handler(void) {
  // Clear interrupt flag
  DMA2->LIFCR = DMA_LIFCR_CTCIF2;

  llspi_disable();

  spi_rx_done();
}

// panda -> master DMA finished
static void DMA2_Stream3_IRQ_Handler(void) {
  ENTER_CRITICAL();

  DMA2->LIFCR = DMA_LIFCR_CTCIF3;
  spi_tx_dma_done = true;

  EXIT_CRITICAL();
}

// panda TX finished
static void SPI4_IRQ_Handler(void) {
  // clear flag
  SPI4->IFCR |= (0x1FFU << 3U);

  if ((SPI4->SR & SPI_SR_TXC) != 0U) {
    if (spi_tx_dma_done) {
      llspi_disable();

      spi_tx_dma_done = false;
      spi_tx_done(false);
    } else if (spi_state == SPI_STATE_DATA_TX) {
      // spurious TXC interrupt
      #ifdef DEBUG_SPI
        print("SPI: spurious TXC\n");
        llspi_dump_state();
      #endif
    }
  } 
  
  if ((SPI4->SR & SPI_SR_UDR) != 0U) {
    // under-run occurred
    #ifdef DEBUG_SPI
      print("SPI: underrun "); puth(spi_tx_dma_done); print("\n");
      llspi_dump_state();
    #endif
  } 
  
  if ((SPI4->SR & SPI_SR_OVR) != 0U) {
    // over-run occurred
    #ifdef DEBUG_SPI
      print("SPI: overrun\n");
      llspi_dump_state();
    #endif
  }
}


void llspi_init(void) {
  REGISTER_INTERRUPT(SPI4_IRQn, SPI4_IRQ_Handler, (SPI_IRQ_RATE * 2U), FAULT_INTERRUPT_RATE_SPI)
  REGISTER_INTERRUPT(DMA2_Stream2_IRQn, DMA2_Stream2_IRQ_Handler, SPI_IRQ_RATE, FAULT_INTERRUPT_RATE_SPI_DMA)
  REGISTER_INTERRUPT(DMA2_Stream3_IRQn, DMA2_Stream3_IRQ_Handler, SPI_IRQ_RATE, FAULT_INTERRUPT_RATE_SPI_DMA)

  // Setup MOSI DMA
  register_set(&(DMAMUX1_Channel10->CCR), 83U, 0xFFFFFFFFU);
  register_set(&(DMA2_Stream2->CR), (DMA_SxCR_MINC | DMA_SxCR_TCIE), 0x1E077EFEU);
  register_set(&(DMA2_Stream2->PAR), (uint32_t)&(SPI4->RXDR), 0xFFFFFFFFU);

  // Setup MISO DMA, memory -> peripheral
  register_set(&(DMAMUX1_Channel11->CCR), 84U, 0xFFFFFFFFU);
  register_set(&(DMA2_Stream3->CR), (DMA_SxCR_MINC | DMA_SxCR_DIR_0 | DMA_SxCR_TCIE), 0x1E077EFEU);
  register_set(&(DMA2_Stream3->PAR), (uint32_t)&(SPI4->TXDR), 0xFFFFFFFFU);

  // Enable SPI
  register_set(&(SPI4->IER), SPI_IER_OVRIE | SPI_IER_UDRIE, 0x3FFU);
  register_set(&(SPI4->CFG1), (7U << SPI_CFG1_DSIZE_Pos), SPI_CFG1_DSIZE_Msk);
  register_set(&(SPI4->UDRDR), 0xcd, 0xFFFFU);  // set under-run value for debugging
  register_set(&(SPI4->CR1), SPI_CR1_SPE, 0xFFFFU);
  register_set(&(SPI4->CR2), 0, 0xFFFFU);

  NVIC_EnableIRQ(DMA2_Stream2_IRQn);
  NVIC_EnableIRQ(DMA2_Stream3_IRQn);
  NVIC_EnableIRQ(SPI4_IRQn);
}
