static uint8_t *llspi_rx_addr;
static int llspi_rx_len;
static int llspi_tx_len;

static void llspi_disable(void) {
  // disable DMA + SPI
  DMA2_Stream2->CR &= ~DMA_SxCR_EN;
  DMA2_Stream3->CR &= ~DMA_SxCR_EN;
  while((DMA2_Stream2->CR & DMA_SxCR_EN) != 0U);
  while((DMA2_Stream3->CR & DMA_SxCR_EN) != 0U);
  SPI4->CR1 &= ~SPI_CR1_SPE;
  DMA2->LIFCR = DMA_LIFCR_CTCIF2 | DMA_LIFCR_CTCIF3;
  register_clear_bits(&(SPI4->CFG1), SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN);
}

void llspi_dma(uint8_t *tx_addr, int tx_len, uint8_t *rx_addr, int rx_len) {
  // set global for later use
  llspi_rx_addr = rx_addr;
  llspi_rx_len = rx_len;
  llspi_tx_len = tx_len;

  // drain the bus
  while ((SPI4->SR & SPI_SR_RXP) != 0U) {
    volatile uint8_t dat = SPI4->RXDR;
    (void)dat;
  }

  // memset(llspi_rx_addr, 0xAA, 0x50);

  // print("TX: "); hexdump(tx_addr, tx_len);

  // clear all pending
  SPI4->IFCR |= (0x1FFU << 3U);
  register_clear_bits(&(SPI4->IER), SPI_IER_EOTIE);

  // setup destinations and length
  register_set_bits(&(SPI4->CFG1), SPI_CFG1_RXDMAEN);
  if (tx_len > 0) {
    register_set(&(DMA2_Stream3->M0AR), (uint32_t)tx_addr, 0xFFFFFFFFU);
    DMA2_Stream3->NDTR = tx_len;
    DMA2_Stream3->CR |= DMA_SxCR_EN;
    register_clear_bits(&(SPI4->CFG2), SPI_CFG2_COMM);
  } else {
    register_set(&(SPI4->CFG2), SPI_CFG2_COMM_1, SPI_CFG2_COMM);
  }

  register_set(&(DMA2_Stream2->M0AR), (uint32_t)rx_addr, 0xFFFFFFFFU);
  int total_len = tx_len + rx_len;
  DMA2_Stream2->NDTR = total_len;
  DMA2_Stream2->CR |= DMA_SxCR_EN;

  SPI4->CR2 = total_len;

  if (tx_len > 0) {
    register_set_bits(&(SPI4->CFG1), SPI_CFG1_TXDMAEN);
  }

  // setup interrupt on EOT and start transfer
  register_set_bits(&(SPI4->IER), SPI_IER_EOTIE);
  SPI4->CR1 |= SPI_CR1_SPE;
}

// panda TX finished
static void SPI4_IRQ_Handler(void) {
  uint32_t sr = SPI4->SR;
  SPI4->IFCR |= (0x1FFU << 3U);

  if (((sr & SPI_SR_EOT) != 0U)) {
    // print("RX DMA: "); puth4(DMA2_Stream2->NDTR); print(" CR: "); puth4(DMA2_Stream2->CR); print("\n");
    llspi_disable();
    // print("SR: "); puth4(sr); print("\n");

    if ((((sr & SPI_SR_RXPLVL) >> SPI_SR_RXPLVL_Pos) == 0U) && (((sr & SPI_SR_RXWNE) == 0U))) {
      // shift any received data down in the rx buffer
      (void)memcpy(llspi_rx_addr, &((uint8_t *)llspi_rx_addr)[llspi_tx_len], llspi_rx_len);

      // print("RX: "); hexdump(llspi_rx_addr, llspi_rx_len);      
      spi_done();
    } else {
      // print("HAVE TO RESET\n");
      // print("RX: "); hexdump(&((uint8_t *)llspi_rx_addr)[llspi_tx_len], llspi_rx_len);
      spi_reset();
    }
  }
}


void llspi_init(void) {
  REGISTER_INTERRUPT(SPI4_IRQn, SPI4_IRQ_Handler, (SPI_IRQ_RATE * 2U), FAULT_INTERRUPT_RATE_SPI)

  // Setup MOSI DMA
  register_set(&(DMAMUX1_Channel10->CCR), 83U, 0xFFFFFFFFU);
  register_set(&(DMA2_Stream2->CR), DMA_SxCR_MINC, 0x1E077EFEU);
  register_set(&(DMA2_Stream2->PAR), (uint32_t)&(SPI4->RXDR), 0xFFFFFFFFU);

  // Setup MISO DMA, memory -> peripheral
  register_set(&(DMAMUX1_Channel11->CCR), 84U, 0xFFFFFFFFU);
  register_set(&(DMA2_Stream3->CR), (DMA_SxCR_MINC | DMA_SxCR_DIR_0), 0x1E077EFEU);
  register_set(&(DMA2_Stream3->PAR), (uint32_t)&(SPI4->TXDR), 0xFFFFFFFFU);

  // Enable SPI
  register_set(&(SPI4->IER), 0U, 0x3FFU);
  register_set(&(SPI4->CFG1), (7U << SPI_CFG1_DSIZE_Pos), SPI_CFG1_DSIZE_Msk);
  register_set(&(SPI4->CFG2), 0U, 0xF7FE80FFU);
  register_set(&(SPI4->UDRDR), 0xcd, 0xFFFFU);  // set under-run value for debugging
  SPI4->CR2 = 0U;
  llspi_disable();

  NVIC_EnableIRQ(SPI4_IRQn);
}
