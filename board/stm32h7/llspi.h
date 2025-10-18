#include "board/drivers/spi_declarations.h"

// master -> panda DMA start
void llspi_mosi_dma(uint8_t *addr, int len) {
  // disable DMA + SPI
  register_clear_bits(&(SPI4->CFG1), SPI_CFG1_RXDMAEN);
  DMA2_Stream2->CR &= ~DMA_SxCR_EN;
  register_clear_bits(&(SPI4->CR1), SPI_CR1_SPE);
  register_set(&(SPI4->UDRDR), 0xdd, 0xFFFFU);  // set under-run value for debugging

  // drain the bus
  while ((SPI4->SR & SPI_SR_RXP) != 0U) {
    volatile uint8_t dat = SPI4->RXDR;
    (void)dat;
  }

  // clear all pending
  SPI4->IFCR |= (0x1FFU << 3U);
  register_set(&(SPI4->IER), 0, 0x3FFU);

  // setup destination and length
  register_set(&(DMA2_Stream2->M0AR), (uint32_t)addr, 0xFFFFFFFFU);
  DMA2_Stream2->NDTR = len;

  // enable DMA + SPI
  DMA2_Stream2->CR |= DMA_SxCR_EN;
  register_set_bits(&(SPI4->CFG1), SPI_CFG1_RXDMAEN);
  register_set_bits(&(SPI4->CR1), SPI_CR1_SPE);
}

static uint32_t dma_len = 0;
static uint32_t dma_start_ts = 0;

// panda -> master DMA start
void llspi_miso_dma(uint8_t *addr, int len) {
  // disable DMA + SPI
  DMA2_Stream3->CR &= ~DMA_SxCR_EN;
  register_clear_bits(&(SPI4->CFG1), SPI_CFG1_TXDMAEN);
  register_clear_bits(&(SPI4->CR1), SPI_CR1_SPE);
  register_set(&(SPI4->UDRDR), 0xcc, 0xFFFFU);  // set under-run value for debugging

  //if (len < 0x44) len = 0x44;
  // setup source and length
  register_set(&(DMA2_Stream3->M0AR), (uint32_t)addr, 0xFFFFFFFFU);
  DMA2_Stream3->NDTR = len;
  dma_len = (uint32_t)len;

  // clear under-run while we were reading
  SPI4->IFCR |= (0x1FFU << 3U);

  // setup interrupt on TXC
  register_set(&(SPI4->IER), (1U << SPI_IER_EOTIE_Pos), 0x3FFU);

  // enable DMA + SPI
  register_set_bits(&(SPI4->CFG1), SPI_CFG1_TXDMAEN);
  DMA2_Stream3->CR |= DMA_SxCR_EN;
  register_set_bits(&(SPI4->CR1), SPI_CR1_SPE);
  dma_start_ts = microsecond_timer_get();
}

static uint32_t dma_done_ts = 0;
static bool spi_tx_dma_done = false;
// master -> panda DMA finished
static void DMA2_Stream2_IRQ_Handler(void) {
  // Clear interrupt flag
  DMA2->LIFCR = DMA_LIFCR_CTCIF2;

  spi_rx_done();
}

// panda -> master DMA finished
static void DMA2_Stream3_IRQ_Handler(void) {
  dma_done_ts = microsecond_timer_get();
  ENTER_CRITICAL();

  DMA2->LIFCR = DMA_LIFCR_CTCIF3;
  //spi_tx_dma_done = true;

  bool timed_out = false;
  while (!(SPI4->SR & SPI_SR_TXC)) {
    if (get_ts_elapsed(microsecond_timer_get(), dma_done_ts) > SPI_TIMEOUT_US) {
      timed_out = true;
      break;
    }
  }
  uint32_t txc = get_ts_elapsed(microsecond_timer_get(), dma_done_ts);
  uint32_t es = get_ts_elapsed(dma_done_ts, dma_start_ts);
  print("DMA end-start: 0x"); puth4(es); print(", TXC 0x"); puth4(txc); print(", tot 0x"); puth4(txc+es); print(", len 0x"); puth4(dma_len); print("\n");
  if (timed_out) print("timed out!!\n");

  // clear out FIFO
  volatile uint8_t dat = SPI4->TXDR;
  (void)dat;
  (void)dma_start_ts;
  (void)dat;
  spi_tx_done(false);

  EXIT_CRITICAL();
}

// panda TX finished
static void SPI4_IRQ_Handler(void) {
  // clear flag
  SPI4->IFCR |= (0x1FFU << 3U);

  if (spi_tx_dma_done && ((SPI4->SR & SPI_SR_TXC) != 0U)) {
    uint32_t now = microsecond_timer_get();
    uint32_t diff = get_ts_elapsed(now, dma_done_ts);
    print("diff: 0x"); puth4(diff); print("\n");
    spi_tx_dma_done = false;
    spi_tx_done(false);
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
  register_set(&(SPI4->IER), 0, 0x3FFU);
  register_set(&(SPI4->CFG1), (7U << SPI_CFG1_DSIZE_Pos) | (0b01 << SPI_CFG1_UDRDET_Pos), SPI_CFG1_DSIZE_Msk | SPI_CFG1_UDRDET_Msk);
  register_set(&(SPI4->CR1), SPI_CR1_SPE, 0xFFFFU);
  register_set(&(SPI4->CR2), 0, 0xFFFFU);

  NVIC_EnableIRQ(DMA2_Stream2_IRQn);
  NVIC_EnableIRQ(DMA2_Stream3_IRQn);
  NVIC_EnableIRQ(SPI4_IRQn);
}
