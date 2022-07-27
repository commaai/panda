#define SPI_BUF_SIZE 1024U
uint8_t spi_buf_rx[SPI_BUF_SIZE];
uint8_t spi_buf_tx[SPI_BUF_SIZE];

#define SPI_SYNC_BYTE 0x5A
#define SPI_ACK 0x79U
#define SPI_NACK 0x1FU

#define SPI_RX_STATE_HEADER 0U
#define SPI_RX_STATE_HEADER_ACK 1U
#define SPI_RX_STATE_HEADER_NACK 2U
#define SPI_RX_STATE_HEADER_ACKED 3U
#define SPI_RX_STATE_HEADER_NACKED 4U
#define SPI_RX_STATE_DATA_RX 5U
#define SPI_RX_STATE_DATA_RX_ACK 6U
#define SPI_RX_STATE_DATA_TX 7U

uint8_t spi_state = SPI_RX_STATE_HEADER;
uint8_t spi_endpoint;
uint16_t spi_data_len_mosi;
uint16_t spi_data_len_miso;

#define SPI_HEADER_SIZE 7U

void spi_miso_dma(uint8_t *addr, int len) {
  // disable DMA
  register_clear_bits(&(SPI1->CR2), SPI_CR2_TXDMAEN);
  DMA2_Stream3->CR &= ~DMA_SxCR_EN;

  // setup source and length
  register_set(&(DMA2_Stream3->M0AR), (uint32_t)addr, 0xFFFFFFFFU);
  DMA2_Stream3->NDTR = len;

  // enable DMA
  DMA2_Stream3->CR |= DMA_SxCR_EN;
  register_set_bits(&(SPI1->CR2), SPI_CR2_TXDMAEN);
}

void spi_mosi_dma(uint8_t *addr, int len) {
  // disable DMA
  register_clear_bits(&(SPI1->CR2), SPI_CR2_RXDMAEN);
  DMA2_Stream2->CR &= ~DMA_SxCR_EN;

  // drain the bus
  volatile uint8_t dat = SPI1->DR;
  (void)dat;

  // setup destination and length
  register_set(&(DMA2_Stream2->M0AR), (uint32_t)addr, 0xFFFFFFFFU);
  DMA2_Stream2->NDTR = len;
  
  // enable DMA
  DMA2_Stream2->CR |= DMA_SxCR_EN;
  register_set_bits(&(SPI1->CR2), SPI_CR2_RXDMAEN);
}

bool check_checksum(uint8_t *data, uint16_t len) {
  // TODO: can speed this up by casting the bulk to uint32_t and xor-ing the bytes afterwards
  uint8_t checksum = 0U;
  for(uint16_t i = 0U; i < len; i++){
    checksum ^= data[i];
  }
  return checksum == 0U;
}

// SPI MOSI DMA FINISHED
void DMA2_Stream2_IRQ_Handler(void) {
  // Clear interrupt flag
  DMA2->LIFCR = DMA_LIFCR_CTCIF2;

  uint8_t next_rx_state = SPI_RX_STATE_HEADER;

  // parse header
  spi_endpoint = spi_buf_rx[1];
  spi_data_len_mosi = spi_buf_rx[2] << 8 | spi_buf_rx[3];
  spi_data_len_miso = spi_buf_rx[4] << 8 | spi_buf_rx[5];

  if (spi_state == SPI_RX_STATE_HEADER) {
    if (spi_buf_rx[0] == SPI_SYNC_BYTE && check_checksum(spi_buf_rx, SPI_HEADER_SIZE)) {
      // response: ACK and start receiving data portion
      spi_buf_tx[0] = SPI_ACK;
      next_rx_state = SPI_RX_STATE_HEADER_ACK;
    } else {
      // response: NACK and reset state machine
      puts("SPI: incorrect header sync or checksum\n");
      spi_buf_tx[0] = SPI_NACK;
      next_rx_state = SPI_RX_STATE_HEADER_NACK;
    }
    spi_miso_dma(spi_buf_tx, 1);
  }

  if (spi_state == SPI_RX_STATE_HEADER_ACKED) {
    spi_mosi_dma(spi_buf_rx + SPI_HEADER_SIZE, spi_data_len_mosi + 1);
    next_rx_state = SPI_RX_STATE_DATA_RX;
  }

  if (spi_state == SPI_RX_STATE_HEADER_NACKED) {
    // Reset state
    spi_mosi_dma(spi_buf_rx, SPI_HEADER_SIZE);
    next_rx_state = SPI_RX_STATE_HEADER;
  }

  if (spi_state == SPI_RX_STATE_DATA_RX) {
    // We got everything! Based on the endpoint specified, call the appropriate handler
    uint16_t response_len = 0U;
    bool reponse_ack = false;
    if (check_checksum(spi_buf_rx + SPI_HEADER_SIZE, spi_data_len_mosi + 1)) {
      if (spi_endpoint == 0U) {
        if (spi_data_len_mosi >= 8U) {
          response_len = comms_control_handler((ControlPacket_t *)(spi_buf_rx + SPI_HEADER_SIZE), spi_buf_tx + 3);
          reponse_ack = true;
        } else {
          puts("SPI: insufficient data for control handler\n");
        }
      } else if (spi_endpoint == 1U) {
        if (spi_data_len_mosi == 0U) {
          response_len = comms_can_read(spi_buf_tx + 3, spi_data_len_miso);
          reponse_ack = true;
        } else {
          puts("SPI: did not expect data for can_read\n");
        }
      } else if (spi_endpoint == 2U) {
        comms_endpoint2_write(spi_buf_rx + SPI_HEADER_SIZE, spi_data_len_mosi);
        reponse_ack = true;
      } else if (spi_endpoint == 3U) {
        if (spi_data_len_mosi > 0U) {
          comms_can_write(spi_buf_rx + SPI_HEADER_SIZE, spi_data_len_mosi);
          reponse_ack = true;
        } else {
          puts("SPI: did expect data for can_write\n");
        }
      }
    } else {
      // Checksum was incorrect
      reponse_ack = false;
      puts("SPI: incorrect data checksum\n");
    }

    // Setup response header
    spi_buf_tx[0] = reponse_ack ? SPI_ACK : SPI_NACK;
    spi_buf_tx[1] = (response_len >> 8) & 0xFFU;
    spi_buf_tx[2] = response_len & 0xFFU;

    // Add checksum
    uint8_t checksum = 0U;
    for(uint16_t i = 0U; i < response_len + 3; i++) {
      checksum ^= spi_buf_tx[i];
    }
    spi_buf_tx[response_len + 3] = checksum;

    // Write response
    spi_miso_dma(spi_buf_tx, response_len + 4);

    next_rx_state = SPI_RX_STATE_DATA_TX;
  }

  spi_state = next_rx_state;
}

// SPI MISO DMA FINISHED
void DMA2_Stream3_IRQ_Handler(void) {
  // Clear interrupt flag
  DMA2->LIFCR = DMA_LIFCR_CTCIF3;

  if (spi_state == SPI_RX_STATE_HEADER_ACK) {
    // ACK was sent, queue up the RX buf for the data + checksum
    spi_mosi_dma(spi_buf_rx, 1U);
    spi_state = SPI_RX_STATE_HEADER_ACKED;
  }

  if (spi_state == SPI_RX_STATE_HEADER_NACK) {
    spi_mosi_dma(spi_buf_rx, 1U);
    spi_state = SPI_RX_STATE_HEADER_NACKED;
  }

  if (spi_state == SPI_RX_STATE_DATA_TX) {
    // Reset state
    spi_mosi_dma(spi_buf_rx, SPI_HEADER_SIZE);
    spi_state = SPI_RX_STATE_HEADER;
  }
}

// ***************************** SPI init *****************************
void spi_init(void) {
  // We expect less than 50 transactions (including control messages and CAN buffers) at the 100Hz boardd interval. Can be raised if needed.
  REGISTER_INTERRUPT(DMA2_Stream2_IRQn, DMA2_Stream2_IRQ_Handler, 5000U, FAULT_INTERRUPT_RATE_SPI_DMA)
  REGISTER_INTERRUPT(DMA2_Stream3_IRQn, DMA2_Stream3_IRQ_Handler, 5000U, FAULT_INTERRUPT_RATE_SPI_DMA)

  // Clear buffers (for debugging)
  memset(spi_buf_rx, 0, SPI_BUF_SIZE);
  memset(spi_buf_tx, 0, SPI_BUF_SIZE);

  // Setup MOSI DMA
  register_set(&(DMA2_Stream2->CR), (DMA_SxCR_CHSEL_1 | DMA_SxCR_CHSEL_0 | DMA_SxCR_MINC | DMA_SxCR_TCIE), 0x1E077EFEU);
  register_set(&(DMA2_Stream2->PAR), (uint32_t)&(SPI1->DR), 0xFFFFFFFFU);

  // Setup MISO DMA
  register_set(&(DMA2_Stream3->CR), (DMA_SxCR_CHSEL_1 | DMA_SxCR_CHSEL_0 | DMA_SxCR_MINC | DMA_SxCR_DIR_0 | DMA_SxCR_TCIE), 0x1E077EFEU);
  register_set(&(DMA2_Stream3->PAR), (uint32_t)&(SPI1->DR), 0xFFFFFFFFU);

  // Enable SPI and the error interrupts
  // TODO: verify clock phase and polarity
  register_set(&(SPI1->CR1), SPI_CR1_SPE, 0xFFFFU);
  register_set(&(SPI1->CR2), 0U, 0xF7U);

  // Start the first packet!
  spi_state = SPI_RX_STATE_HEADER;
  spi_mosi_dma(spi_buf_rx, SPI_HEADER_SIZE);

  NVIC_EnableIRQ(DMA2_Stream2_IRQn);
  NVIC_EnableIRQ(DMA2_Stream3_IRQn);
}
