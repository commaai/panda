/* SPI transfer:
    B0: Sync (0x5A)
    B1: Endpoint
    B2: tx_len MSB
    B3: tx_len LSB
    B4-B(4+tx_len): Data
    B(4+tx_len+1): CRC8

  <keep sending 0x00 while (waiting to) rx>

  Response:
    B0: 0x79 (ACK) or 0x1F (NACK)
    B1: rx_len MSB
    B2: rx_len LSB
    B3-B(3+rx_len): Data
    B(3+rx_len_1): CRC8
*/

#define SPI_BUF_SIZE 1024U
uint8_t spi_buf_rx[SPI_BUF_SIZE];
uint8_t spi_buf_tx[SPI_BUF_SIZE];

#define SPI_SYNC_BYTE 0x5A
#define SPI_ACK 0x79U
#define SPI_NACK 0x1FU

#define SPI_RX_STATE_IDLE 0U
#define SPI_RX_STATE_HEADER 1U
#define SPI_RX_STATE_DATA 2U
uint8_t spi_rx_state = SPI_RX_STATE_IDLE;

void spi_tx_dma(uint8_t *addr, int len) {
  // disable DMA
  register_clear_bits(&(SPI1->CR2), SPI_CR2_TXDMAEN);
  DMA2_Stream3->CR &= ~DMA_SxCR_EN;

  // DMA2, stream 3, channel 3
  register_set(&(DMA2_Stream3->M0AR), (uint32_t)addr, 0xFFFFFFFFU);
  DMA2_Stream3->NDTR = len;
  register_set(&(DMA2_Stream3->PAR), (uint32_t)&(SPI1->DR), 0xFFFFFFFFU);

  // channel3, increment memory, memory -> periph, enable
  register_set(&(DMA2_Stream3->CR), (DMA_SxCR_CHSEL_1 | DMA_SxCR_CHSEL_0 | DMA_SxCR_MINC | DMA_SxCR_DIR_0), 0x1E077EFEU);
  DMA2_Stream3->CR |= DMA_SxCR_EN;
  delay(0);
  register_set_bits(&(DMA2_Stream3->CR), DMA_SxCR_TCIE);

  register_set_bits(&(SPI1->CR2), SPI_CR2_TXDMAEN);
}

void spi_rx_dma(uint8_t *addr, int len) {
  // disable DMA and RX interrupt
  register_clear_bits(&(SPI1->CR2), SPI_CR2_RXDMAEN | SPI_CR2_RXNEIE);
  DMA2_Stream2->CR &= ~DMA_SxCR_EN;

  // drain the bus
  // volatile uint8_t dat = SPI1->DR;
  // (void)dat;

  puts("DMA2: want "); puth(len); puts(" bytes\n");

  // DMA2, stream 2, channel 3
  register_set(&(DMA2_Stream2->M0AR), (uint32_t)addr, 0xFFFFFFFFU);
  DMA2_Stream2->NDTR = len;
  register_set(&(DMA2_Stream2->PAR), (uint32_t)&(SPI1->DR), 0xFFFFFFFFU);

  // channel3, increment memory, periph -> memory, enable
  register_set(&(DMA2_Stream2->CR), (DMA_SxCR_CHSEL_1 | DMA_SxCR_CHSEL_0 | DMA_SxCR_MINC), 0x1E077EFEU);
  DMA2_Stream2->CR |= DMA_SxCR_EN;
  delay(0);
  register_set_bits(&(DMA2_Stream2->CR), DMA_SxCR_TCIE);

  register_set_bits(&(SPI1->CR2), SPI_CR2_RXDMAEN);
  puts("DMA2 enabled\n");
}

// SPI RX
void DMA2_Stream2_IRQ_Handler(void) {
  uint8_t next_rx_state = SPI_RX_STATE_IDLE;

  uint8_t endpoint = spi_buf_rx[0];
  uint16_t data_len = (spi_buf_rx[1] << 8) | spi_buf_rx[2];

  uint16_t tx_len = 0U;
  bool tx_ack = false;

  puts("DMA2 IRQ\n");

  if (spi_rx_state == SPI_RX_STATE_HEADER) {
    puts("SPI: got header "); puth(endpoint); puts(" "); puth(data_len); puts("\n");
    // start receiving data + CRC
    next_rx_state = SPI_RX_STATE_DATA;
    spi_rx_dma(spi_buf_rx + 3, data_len + 1);
  }

  if (spi_rx_state == SPI_RX_STATE_DATA) {
    puts("SPI: got data\n");
    // TODO: verify CRC

    // We got everything! Based on the endpoint specified, call the appropriate handler
    if (endpoint == 0U) {
      if (data_len >= 8U) {
        tx_len = comms_control_handler((ControlPacket_t *)(spi_buf_rx + 3), spi_buf_tx + 3);
        tx_ack = true;
      } else {
        puts("SPI: insufficient data for control handler\n");
      }
    } else if (endpoint == 1U) {
      if (data_len == 0U) {
        tx_len = comms_can_read(spi_buf_tx + 3, SPI_BUF_SIZE - 4);
        tx_ack = true;
      } else {
        puts("SPI: did not expect data for can_read\n");
      }
    } else if (endpoint == 2U) {
      comms_endpoint2_write(spi_buf_tx + 3, data_len);
      tx_ack = true;
    } else if (endpoint == 3U) {
      if (data_len > 0U) {
        comms_can_write(spi_buf_tx + 3, data_len);
        tx_ack = true;
      } else {
        puts("SPI: did expect data for can_write\n");
      }
    }

    // Setup response header
    spi_buf_tx[0] = tx_ack ? SPI_ACK : SPI_NACK;
    spi_buf_tx[1] = (tx_len >> 8) & 0xFFU;
    spi_buf_tx[2] = tx_len & 0xFFU;

    // Add CRC
    uint8_t checksum = 0U;
    for(uint16_t i = 0U; i < tx_len + 3; i++) {
      checksum ^= spi_buf_tx[i];
    }
    spi_buf_tx[tx_len + 3] = checksum;

    // Write response
    spi_tx_dma(spi_buf_tx, tx_len + 4);

    next_rx_state = SPI_RX_STATE_IDLE;
  }

  // Clear interrupt flag
  DMA2->LIFCR = DMA_LIFCR_CTCIF2;

  // Re-enable RX interrupt if idle
  if (next_rx_state == SPI_RX_STATE_IDLE) {
    puts("Re-enable RXNE\n");
    register_set_bits(&(SPI1->CR2), SPI_CR2_RXNEIE);
  }
  spi_rx_state = next_rx_state;
}

// SPI TX
void DMA2_Stream3_IRQ_Handler(void) {
  // Clear interrupt flag
  DMA2->LIFCR = DMA_LIFCR_CTCIF3;
}

void SPI1_IRQ_Handler(void) {
  if (SPI1->SR & SPI_SR_RXNE) {
    uint8_t dat = SPI1->DR;
    // puts("SPI: got something: 0x"); puth(dat); puts("\n");
    if (spi_rx_state == SPI_RX_STATE_IDLE && dat == SPI_SYNC_BYTE) {
      // puts("SPI: got sync"); puts("\n");
      // Start receiving the rest of the header
      spi_rx_state = SPI_RX_STATE_HEADER;
      spi_rx_dma(spi_buf_rx, 3);
    }
  }

  if (SPI1->SR & SPI_SR_CRCERR) {
    // CRC error
    puts("SPI: CRC error\n");
    SPI1->SR &= ~(1 << SPI_SR_CRCERR);
  }

  if (SPI1->SR & SPI_SR_OVR) {
    // RX overrun
    // TODO: implement recovery if neccesary (reading DR and SR)
    puts("SPI: overrun error\n");
  }
}

// ***************************** SPI init *****************************
void spi_init(void) {
  // We expect less than 10 transfers (including control messages and CAN buffers) at the 100Hz boardd interval. Can be raised if needed.
  REGISTER_INTERRUPT(DMA2_Stream2_IRQn, DMA2_Stream2_IRQ_Handler, 1000U, FAULT_INTERRUPT_RATE_SPI_DMA)
  REGISTER_INTERRUPT(DMA2_Stream3_IRQn, DMA2_Stream3_IRQ_Handler, 1000U, FAULT_INTERRUPT_RATE_SPI_DMA)
  REGISTER_INTERRUPT(SPI1_IRQn, SPI1_IRQ_Handler, 1000U, FAULT_INTERRUPT_RATE_SPI)

  // Clear buffers (for debugging)
  memset(spi_buf_rx, 0, SPI_BUF_SIZE);
  memset(spi_buf_tx, 0, SPI_BUF_SIZE);

  // TODO: verify clock phase and polarity
  register_set(&(SPI1->CR1), SPI_CR1_SPE, 0xFFFFU);
  register_set(&(SPI1->CR2), SPI_CR2_ERRIE | SPI_CR2_RXNEIE, 0xF7U);

  NVIC_EnableIRQ(DMA2_Stream2_IRQn);
  NVIC_EnableIRQ(DMA2_Stream3_IRQn);
  NVIC_EnableIRQ(SPI1_IRQn);
}
