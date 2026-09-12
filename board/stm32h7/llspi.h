#pragma once

#ifdef BOOTSTUB
#include "board/stm32h7/llspi_legacy.h"
#else
// NSS rising is the only frame boundary. Both normal DMA streams
// stop at capacity; neither DMA TC nor TXC advances the protocol.
#define LLSPI_CS_MASK (1UL << 11)
#define LLSPI_DMA_FLAGS (DMA_LIFCR_CFEIF2 | DMA_LIFCR_CDMEIF2 | DMA_LIFCR_CTEIF2 | DMA_LIFCR_CHTIF2 | DMA_LIFCR_CTCIF2 | DMA_LIFCR_CFEIF3 | DMA_LIFCR_CDMEIF3 | DMA_LIFCR_CTEIF3 | DMA_LIFCR_CHTIF3 | DMA_LIFCR_CTCIF3)

static uint8_t *llspi_rx_addr;
static uint16_t llspi_rx_capacity;
static bool llspi_receiving;
static bool llspi_armed;
static bool llspi_dma_fault;

static bool llspi_cs_high(void) {
  return (GPIOE->IDR & LLSPI_CS_MASK) != 0U;
}

static bool llspi_stop_dma(void) {
  register_clear_bits(&(SPI4->CFG1), SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN);
  DMA2_Stream2->CR &= ~DMA_SxCR_EN;
  DMA2_Stream3->CR &= ~DMA_SxCR_EN;
  uint32_t remaining = 1000U;
  while ((((DMA2_Stream2->CR | DMA2_Stream3->CR) & DMA_SxCR_EN) != 0U) && (remaining > 0U)) {
    remaining--;
  }
  __DSB();
  llspi_dma_fault = llspi_dma_fault || (remaining == 0U);
  return !llspi_dma_fault;
}

static void llspi_reset(void) {
  // Reset after RX evidence is collected, including TX FIFO and underrun state.
  register_clear_bits(&(SPI4->CR1), SPI_CR1_SPE);
  RCC->APB2RSTR |= RCC_APB2RSTR_SPI4RST;
  __DSB();
  RCC->APB2RSTR &= ~RCC_APB2RSTR_SPI4RST;
  __DSB();
  register_set(&(SPI4->CR1), 0U, 0xFFFFU);
  register_set(&(SPI4->CR2), 0U, 0xFFFFU);
  register_set(&(SPI4->CFG1), 7U << SPI_CFG1_DSIZE_Pos, 0xFFFFFFFFU);
  // Mode 0, hardware NSS, slave, full-duplex peripheral with one useful DMA.
  register_set(&(SPI4->CFG2), 0U, 0xFFFFFFFFU);
  register_set(&(SPI4->IER), 0U, 0x3FFU);
  register_set(&(SPI4->UDRDR), 0U, 0xFFFFU);
  DMA2->LIFCR = LLSPI_DMA_FLAGS;
  llspi_armed = false;
}

static void llspi_enable(void) {
  // A low NSS or a second rising edge means the rearm deadline was missed.
  // Never knowingly enable after NSS fell. The final check also rejects an
  // edge racing enable; the specified minimum CS-high interval is still required.
  if (!llspi_dma_fault && llspi_cs_high() && ((EXTI->PR1 & LLSPI_CS_MASK) == 0U)) {
    register_set_bits(&(SPI4->CR1), SPI_CR1_SPE);
    __DSB();
    llspi_armed = llspi_cs_high() && ((EXTI->PR1 & LLSPI_CS_MASK) == 0U);
  }
  if (!llspi_armed) {
    (void)llspi_stop_dma();
    register_clear_bits(&(SPI4->CR1), SPI_CR1_SPE);
  }
}

// cppcheck-suppress constParameterPointer ; DMA and residual FIFO write addr
void llspi_mosi_dma(uint8_t *addr, int len) {
  llspi_receiving = true;
  llspi_rx_addr = addr;
  llspi_rx_capacity = (uint16_t)len;
  if ((len > 0) && (len <= 4096) && !llspi_dma_fault) {
    register_set(&(DMA2_Stream2->M0AR), (uint32_t)addr, 0xFFFFFFFFU);
    DMA2_Stream2->NDTR = (uint32_t)len;
    DMA2_Stream2->CR |= DMA_SxCR_EN;
    register_set_bits(&(SPI4->CFG1), SPI_CFG1_RXDMAEN);
    llspi_enable();
  }
}

void llspi_miso_dma(const uint8_t *addr, int len) {
  llspi_receiving = false;
  if ((len > 0) && (len <= 4096) && !llspi_dma_fault) {
    register_set(&(DMA2_Stream3->M0AR), (uint32_t)addr, 0xFFFFFFFFU);
    DMA2_Stream3->NDTR = (uint32_t)len;
    DMA2_Stream3->CR |= DMA_SxCR_EN;
    register_set_bits(&(SPI4->CFG1), SPI_CFG1_TXDMAEN);
    llspi_enable();
  }
}

static void llspi_cs_irq_handler(void) {
  if ((EXTI->PR1 & LLSPI_CS_MASK) != 0U) {
    EXTI->PR1 = LLSPI_CS_MASK;
    // A delayed rising IRQ must not reset a newer active transaction. Mark the
    // accumulated frame invalid and wait for its next rising edge to recover.
    if (!llspi_cs_high()) {
      llspi_armed = false;
    } else {
      bool valid = llspi_armed;
      if (llspi_stop_dma()) {
        uint16_t rx_len = 0U;
        if (llspi_receiving) {
          uint32_t dma_errors = DMA2->LISR & (DMA_LISR_TEIF2 | DMA_LISR_DMEIF2 | DMA_LISR_FEIF2);
          uint32_t count = (uint32_t)llspi_rx_capacity - DMA2_Stream2->NDTR;
          uint32_t status = SPI4->SR;
          valid = valid && (dma_errors == 0U) && ((status & SPI_SR_OVR) == 0U);
          // Keep SPE set until residual data is drained. Byte accesses avoid
          // consuming multiple packed bytes while accounting only one.
          uint32_t budget = 32U;
          while (((SPI4->SR & (SPI_SR_RXP | SPI_SR_RXWNE | SPI_SR_RXPLVL)) != 0U) && (budget > 0U)) {
            uint8_t data = *((volatile uint8_t *)&SPI4->RXDR);
            if (count < llspi_rx_capacity) {
              llspi_rx_addr[count] = data;
            }
            count++;
            budget--;
          }
          valid = valid && (budget > 0U) && (count <= llspi_rx_capacity) && ((SPI4->SR & SPI_SR_OVR) == 0U);
          rx_len = (uint16_t)count;
        }
        if (llspi_cs_high()) {
          valid = valid && ((EXTI->PR1 & LLSPI_CS_MASK) == 0U);
          llspi_reset();
          spi_cs_end(rx_len, valid);
        } else {
          llspi_armed = false;
        }
      }
      // DMA failure is latched fail-closed: do not reuse memory it may own.
    }
  }
}

void llspi_init(void) {
  REGISTER_INTERRUPT(EXTI15_10_IRQn, llspi_cs_irq_handler, (SPI_IRQ_RATE * 2U), FAULT_INTERRUPT_RATE_SPI)
  register_set(&(DMAMUX1_Channel10->CCR), 83U, 0xFFFFFFFFU);
  register_set(&(DMAMUX1_Channel11->CCR), 84U, 0xFFFFFFFFU);
  register_set(&(DMA2_Stream2->CR), DMA_SxCR_MINC, 0x1E077EFEU);
  register_set(&(DMA2_Stream3->CR), DMA_SxCR_MINC | DMA_SxCR_DIR_0, 0x1E077EFEU);
  register_set(&(DMA2_Stream2->PAR), (uint32_t)&SPI4->RXDR, 0xFFFFFFFFU);
  register_set(&(DMA2_Stream3->PAR), (uint32_t)&SPI4->TXDR, 0xFFFFFFFFU);
  llspi_reset();
  // EXTI11 can observe PE11 while its pin remains the SPI4 NSS alternate function.
  // EXTI12 shares this vector only for CAN stop-mode wakeup (which resets).
  register_set(&(SYSCFG->EXTICR[2]), SYSCFG_EXTICR3_EXTI11_PE, SYSCFG_EXTICR3_EXTI11);
  register_clear_bits(&(EXTI->FTSR1), LLSPI_CS_MASK);
  register_set_bits(&(EXTI->RTSR1), LLSPI_CS_MASK);
  EXTI->PR1 = LLSPI_CS_MASK;
  register_set_bits(&(EXTI->IMR1), LLSPI_CS_MASK);
  NVIC_EnableIRQ(EXTI15_10_IRQn);
}
#endif
