#pragma once

#define LLSPI_CS_MASK (1UL << 11U)
#define LLSPI_DMA_FLAGS (DMA_LIFCR_CFEIF2 | DMA_LIFCR_CDMEIF2 | DMA_LIFCR_CTEIF2 | DMA_LIFCR_CHTIF2 | DMA_LIFCR_CTCIF2 | DMA_LIFCR_CFEIF3 | DMA_LIFCR_CDMEIF3 | DMA_LIFCR_CTEIF3 | DMA_LIFCR_CHTIF3 | DMA_LIFCR_CTCIF3)

static uint16_t llspi_rx_offset = 0U;
static bool llspi_enable_pending = false;

static bool llspi_cs_high(void) {
  return (GPIOE->IDR & LLSPI_CS_MASK) != 0U;
}

static void llspi_stop(void) {
  register_clear_bits(&(SPI4->CFG1), SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN);
  DMA2_Stream2->CR &= ~DMA_SxCR_EN;
  DMA2_Stream3->CR &= ~DMA_SxCR_EN;
  while (((DMA2_Stream2->CR | DMA2_Stream3->CR) & DMA_SxCR_EN) != 0U) { }
  register_clear_bits(&(SPI4->CR1), SPI_CR1_SPE);
  DMA2->LIFCR = LLSPI_DMA_FLAGS;
  SPI4->IFCR = 0xFF8U;
}

static void llspi_enable(void) {
  llspi_enable_pending = !llspi_cs_high();
  if (!llspi_enable_pending) {
    register_set_bits(&(SPI4->CR1), SPI_CR1_SPE);
  }
}

void llspi_dma(bool reply) {
  llspi_stop();
  llspi_rx_offset = reply ? SPI_FRAME_SIZE : 0U;
  uint16_t total_len = llspi_rx_offset + SPI_FRAME_SIZE;
  (void)memset(&spi_buf_tx[llspi_rx_offset], 0xCD, SPI_FRAME_SIZE);

  // Receive the response's dummy MOSI bytes and the next request in one DMA.
  // No interrupt or reconfiguration is needed between these two CS sessions.
  register_set(&(DMA2_Stream2->M0AR), (uint32_t)spi_buf_rx, 0xFFFFFFFFU);
  DMA2_Stream2->NDTR = total_len;
  register_set(&(DMA2_Stream3->M0AR), (uint32_t)spi_buf_tx, 0xFFFFFFFFU);
  DMA2_Stream3->NDTR = total_len;
  register_set(&(SPI4->CR2), total_len, SPI_CR2_TSIZE);
  DMA2_Stream2->CR |= DMA_SxCR_EN;
  DMA2_Stream3->CR |= DMA_SxCR_EN;
  register_set_bits(&(SPI4->CFG1), SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN);
  llspi_enable();
}

static void DMA2_Stream2_IRQ_Handler(void) {
  if ((DMA2->LISR & DMA_LISR_TCIF2) != 0U) {
    llspi_stop();
    spi_rx_done(&spi_buf_rx[llspi_rx_offset]);
  }
}

static void llspi_cs_irq_handler(void) {
  if ((EXTI->PR1 & LLSPI_CS_MASK) != 0U) {
    EXTI->PR1 = LLSPI_CS_MASK;
    if (llspi_cs_high()) {
      if (llspi_enable_pending) {
        llspi_enable();
      } else {
        uint16_t count = llspi_rx_offset + SPI_FRAME_SIZE - DMA2_Stream2->NDTR;
        // cppcheck-suppress knownConditionTrueFalse ; NDTR changes asynchronously as DMA receives bytes
        if ((count % SPI_FRAME_SIZE) != 0U) {
          // CS ended partway through a response or request. Discard that frame.
          if ((count > llspi_rx_offset) && !spi_is_poll(&spi_buf_rx[llspi_rx_offset], count - llspi_rx_offset)) {
            spi_error_count += 1U;
          }
          llspi_dma(false);
        } else {
          // Full frames complete through RX DMA; empty CS pulses have no effect.
        }
      }
    }
  }
}

void llspi_init(void) {
  REGISTER_INTERRUPT(DMA2_Stream2_IRQn, DMA2_Stream2_IRQ_Handler, SPI_IRQ_RATE, FAULT_INTERRUPT_RATE_SPI_DMA)
  REGISTER_INTERRUPT(EXTI15_10_IRQn, llspi_cs_irq_handler, SPI_IRQ_RATE * 2U, FAULT_INTERRUPT_RATE_SPI)
  register_set(&(DMAMUX1_Channel10->CCR), 83U, 0xFFFFFFFFU);
  register_set(&(DMAMUX1_Channel11->CCR), 84U, 0xFFFFFFFFU);
  register_set(&(DMA2_Stream2->CR), DMA_SxCR_MINC | DMA_SxCR_TCIE, 0x1E077EFEU);
  register_set(&(DMA2_Stream3->CR), DMA_SxCR_MINC | DMA_SxCR_DIR_0, 0x1E077EFEU);
  register_set(&(DMA2_Stream2->PAR), (uint32_t)&SPI4->RXDR, 0xFFFFFFFFU);
  register_set(&(DMA2_Stream3->PAR), (uint32_t)&SPI4->TXDR, 0xFFFFFFFFU);
  register_set(&(SPI4->IER), 0U, 0x3FFU);
  register_set(&(SPI4->CFG1), 7U << SPI_CFG1_DSIZE_Pos, SPI_CFG1_DSIZE_Msk);
  register_set(&(SPI4->CFG2), 0U, 0xF7FE80FFU);
  register_set(&(SPI4->UDRDR), 0xCDU, 0xFFFFU);

  // Observe NSS without changing its SPI alternate function.
  register_set(&(SYSCFG->EXTICR[2]), SYSCFG_EXTICR3_EXTI11_PE, SYSCFG_EXTICR3_EXTI11);
  register_clear_bits(&(EXTI->FTSR1), LLSPI_CS_MASK);
  register_set_bits(&(EXTI->RTSR1), LLSPI_CS_MASK);
  EXTI->PR1 = LLSPI_CS_MASK;
  register_set_bits(&(EXTI->IMR1), LLSPI_CS_MASK);
  NVIC_EnableIRQ(DMA2_Stream2_IRQn);
  NVIC_EnableIRQ(EXTI15_10_IRQn);
}
