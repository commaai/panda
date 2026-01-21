
static uint8_t *llspi_rx_addr;
static int llspi_rx_len, llspi_tx_len;
static bool llspi_pending = false;


void llspi_dump_state(void){
  print("    STATE: "); puth(spi_state); print("\n");
  print("    SPI4 CR1: "); puth(SPI4->CR1);
  print(" CR2: "); puth(SPI4->CR2);
  print(" SR: "); puth(SPI4->SR);
  print(" IER: "); puth(SPI4->IER);
  print(" CFG1: "); puth(SPI4->CFG1);
  print(" CFG2: "); puth(SPI4->CFG2);
  print("\n");
  print("    DMA2 Stream2 CR: "); puth(DMA2_Stream2->CR);
  print(" NDTR: "); puth(DMA2_Stream2->NDTR); print("\n");
  print("    DMA2 Stream3 CR: "); puth(DMA2_Stream3->CR);
  print(" NDTR: "); puth(DMA2_Stream3->NDTR); print("\n");
  print(" LISR: "); puth(DMA2->LISR);
  print("\n\n");
}

static void llspi_disable(void) {
  // disable DMA + SPI
  llspi_pending = false;
  DMA2_Stream2->CR &= ~DMA_SxCR_EN;
  DMA2_Stream3->CR &= ~DMA_SxCR_EN;
  while((DMA2_Stream2->CR & DMA_SxCR_EN) != 0U);
  while((DMA2_Stream3->CR & DMA_SxCR_EN) != 0U);
  SPI4->CR1 &= ~SPI_CR1_SPE;
  DMA2->LIFCR = DMA_LIFCR_CTCIF2 | DMA_LIFCR_CTCIF3;
  register_clear_bits(&(SPI4->CFG1), SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN);
}

void llspi_dma(uint8_t *tx_addr, int tx_len, uint8_t *rx_addr, int rx_len) {
  print("\nLLSPI DMA TX len "); puth(tx_len); print(" RX len "); puth(rx_len); print("\n");

  // set global for later use
  llspi_rx_addr = rx_addr;
  llspi_rx_len = rx_len;
  llspi_tx_len = tx_len;

  // memset(rx_addr, 0xAA, 0x100);

  // drain the bus
  while ((SPI4->SR & SPI_SR_RXP) != 0U) {
    volatile uint8_t dat = SPI4->RXDR;
    (void)dat;
  }

  // clear all pending
  SPI4->IFCR |= (0x1FFU << 3U);
  register_clear_bits(&(SPI4->IER), SPI_IER_EOTIE);

  // setup destinations and length
  int total_len = tx_len + rx_len;

  register_set_bits(&(SPI4->CFG1), SPI_CFG1_RXDMAEN);

  if (tx_len > 0) {
    // print("TX: "); hexdump(tx_addr, tx_len);
    register_set(&(DMA2_Stream3->M0AR), (uint32_t)tx_addr, 0xFFFFFFFFU);
    DMA2_Stream3->NDTR = tx_len;
    DMA2_Stream3->CR |= DMA_SxCR_EN;
    register_clear_bits(&(SPI4->CFG2), SPI_CFG2_COMM);
  } else {
    register_set(&(SPI4->CFG2), SPI_CFG2_COMM_1, SPI_CFG2_COMM);
  }

  //llspi_dump_state();
  register_set(&(DMA2_Stream2->M0AR), (uint32_t)rx_addr, 0xFFFFFFFFU);
  DMA2_Stream2->NDTR = total_len;
  DMA2_Stream2->CR |= DMA_SxCR_EN;
  //llspi_dump_state();

  SPI4->CR2 = total_len;
  //SPI4->CR2 = 0U;

  if (tx_len > 0) {
    register_set_bits(&(SPI4->CFG1), SPI_CFG1_TXDMAEN);
  }

  // setup interrupt on EOT
  register_set_bits(&(SPI4->IER), SPI_IER_EOTIE);

  llspi_pending = true;
  SPI4->CR1 |= SPI_CR1_SPE;

  // TODO: check that the rx buffer is large enough
}

// master -> panda DMA finished
static void DMA2_Stream2_IRQ_Handler(void) {
  // Clear interrupt flag
  DMA2->LIFCR = DMA_LIFCR_CTCIF2;
  print("RX DMA done\n");

  // llspi_disable();

  // // shift any received data down in the rx buffer
  // // memcpy(llspi_rx_addr, &((uint8_t *)llspi_rx_addr)[llspi_tx_len], llspi_rx_len);
  // for(uint16_t i = 0U; i < llspi_rx_len; i++) {
  //   ((uint8_t *)llspi_rx_addr)[i] = ((uint8_t *)llspi_rx_addr)[i + llspi_tx_len];
  // }

  // print("RX: "); hexdump(llspi_rx_addr, llspi_rx_len);

  // spi_done();
}

// panda -> master DMA finished
static void DMA2_Stream3_IRQ_Handler(void) {
  DMA2->LIFCR = DMA_LIFCR_CTCIF3;
  print("TX DMA done\n");
}

// panda TX finished
static void SPI4_IRQ_Handler(void) {
  uint32_t sr = SPI4->SR;
  SPI4->IFCR |= (0x1FFU << 3U);

  //print("IRQ SR: "); puth(sr); print("\n");

  if (((sr & SPI_SR_EOT) != 0U) && llspi_pending) {
    // print("RX 1: "); hexdump(llspi_rx_addr, llspi_rx_len);

    if (llspi_rx_addr[0] == 0xAA) {
      llspi_dump_state();
    }

    // shift any received data down in the rx buffer
    //memcpy(llspi_rx_addr, &((uint8_t *)llspi_rx_addr)[llspi_tx_len], llspi_rx_len);
    for(uint16_t i = 0U; i < llspi_rx_len; i++) {
      ((uint8_t *)llspi_rx_addr)[i] = ((uint8_t *)llspi_rx_addr)[i + llspi_tx_len];
    }

    // print("RX 2: "); hexdump(llspi_rx_addr, llspi_rx_len);

    llspi_disable();
    spi_done();
  }
}


void llspi_init(void) {
  REGISTER_INTERRUPT(SPI4_IRQn, SPI4_IRQ_Handler, (SPI_IRQ_RATE * 2U), FAULT_INTERRUPT_RATE_SPI)
  REGISTER_INTERRUPT(DMA2_Stream2_IRQn, DMA2_Stream2_IRQ_Handler, SPI_IRQ_RATE, FAULT_INTERRUPT_RATE_SPI_DMA)
  REGISTER_INTERRUPT(DMA2_Stream3_IRQn, DMA2_Stream3_IRQ_Handler, SPI_IRQ_RATE, FAULT_INTERRUPT_RATE_SPI_DMA)

  // Setup MOSI DMA
  register_set(&(DMAMUX1_Channel10->CCR), 83U, 0xFFFFFFFFU);
  //register_set(&(DMA2_Stream2->CR), (DMA_SxCR_MINC | DMA_SxCR_TCIE), 0x1E077EFEU);
  register_set(&(DMA2_Stream2->CR), DMA_SxCR_MINC, 0x1E077EFEU);
  register_set(&(DMA2_Stream2->PAR), (uint32_t)&(SPI4->RXDR), 0xFFFFFFFFU);

  // Setup MISO DMA, memory -> peripheral
  register_set(&(DMAMUX1_Channel11->CCR), 84U, 0xFFFFFFFFU);
  //register_set(&(DMA2_Stream3->CR), (DMA_SxCR_MINC | DMA_SxCR_DIR_0 | DMA_SxCR_TCIE), 0x1E077EFEU);
  register_set(&(DMA2_Stream3->CR), (DMA_SxCR_MINC | DMA_SxCR_DIR_0), 0x1E077EFEU);
  register_set(&(DMA2_Stream3->PAR), (uint32_t)&(SPI4->TXDR), 0xFFFFFFFFU);

  // Enable SPI
  register_set(&(SPI4->IER), 0U, 0x3FFU);
  register_set(&(SPI4->CFG1), (7U << SPI_CFG1_DSIZE_Pos), SPI_CFG1_DSIZE_Msk);
  register_set(&(SPI4->CFG2), SPI_CFG2_AFCNTR, 0xF7FE80FFU);
  //register_set(&(SPI4->CFG2), SPI_CFG2_SSM, 0xF7FE80FFU);
  //register_set(&(SPI4->CFG2), 0U, 0xF7FE80FFU);
  register_set(&(SPI4->UDRDR), 0xcd, 0xFFFFU);  // set under-run value for debugging
  SPI4->CR2 = 0U;
  llspi_disable();

  NVIC_EnableIRQ(DMA2_Stream2_IRQn);
  NVIC_EnableIRQ(DMA2_Stream3_IRQn);
  NVIC_EnableIRQ(SPI4_IRQn);
}
