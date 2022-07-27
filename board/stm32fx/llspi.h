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

#define SPI_RX_STATE_HEADER 0U
#define SPI_RX_STATE_DATA_RX 1U
#define SPI_RX_STATE_DATA_TX 2U

uint8_t spi_rx_state = SPI_RX_STATE_HEADER;
uint8_t spi_endpoint;
uint16_t spi_data_len_mosi;
uint16_t spi_data_len_miso;

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

// SPI MOSI DMA FINISHED
void DMA2_Stream2_IRQ_Handler(void) {
  // Clear interrupt flag
  DMA2->LIFCR = DMA_LIFCR_CTCIF2;

  uint8_t next_rx_state = SPI_RX_STATE_HEADER; 

  // parse header
  spi_endpoint = spi_buf_rx[1];
  spi_data_len_mosi = spi_buf_rx[2] << 8 | spi_buf_rx[3];
  spi_data_len_miso = spi_buf_rx[4] << 8 | spi_buf_rx[5];

  if (spi_rx_state == SPI_RX_STATE_HEADER) {
    // send (N)ACK
    // TODO: check sync and CRC
    spi_buf_tx[0] = SPI_ACK;
    spi_miso_dma(spi_buf_tx, 1);    

    // start receiving data + CRC
    next_rx_state = SPI_RX_STATE_DATA_RX;
  }

  if (spi_rx_state == SPI_RX_STATE_DATA_RX) {
    // TODO: verify CRC

    // We got everything! Based on the endpoint specified, call the appropriate handler
    uint16_t response_len = 0U;
    bool reponse_ack = false;
    if (spi_endpoint == 0U) {
      if (spi_data_len_mosi >= 8U) {
        response_len = comms_control_handler((ControlPacket_t *)(spi_buf_rx + 8), spi_buf_tx + 3);
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
      comms_endpoint2_write(spi_buf_rx + 8, spi_data_len_mosi);
      reponse_ack = true;
    } else if (spi_endpoint == 3U) {
      if (spi_data_len_mosi > 0U) {
        comms_can_write(spi_buf_tx + 3, spi_data_len_mosi);
        reponse_ack = true;
      } else {
        puts("SPI: did expect data for can_write\n");
      }
    }

    // Setup response header
    spi_buf_tx[0] = reponse_ack ? SPI_ACK : SPI_NACK;
    spi_buf_tx[1] = (response_len >> 8) & 0xFFU;
    spi_buf_tx[2] = response_len & 0xFFU;

    // Add CRC
    uint8_t checksum = 0U;
    for(uint16_t i = 0U; i < response_len + 3; i++) {
      checksum ^= spi_buf_tx[i];
    }
    spi_buf_tx[response_len + 3] = checksum;

    // Write response
    spi_miso_dma(spi_buf_tx, response_len + 4);

    next_rx_state = SPI_RX_STATE_DATA_TX;
  }

  spi_rx_state = next_rx_state;
}

// SPI MISO DMA FINISHED
void DMA2_Stream3_IRQ_Handler(void) {
  // Clear interrupt flag
  DMA2->LIFCR = DMA_LIFCR_CTCIF3;

  if (spi_rx_state == SPI_RX_STATE_DATA_RX) {
    // ACK was sent, queue up the RX buf for the data + CRC
    spi_mosi_dma(spi_buf_rx + 7, spi_data_len_mosi + 2);
  }

  if (spi_rx_state == SPI_RX_STATE_DATA_TX) {
    // Reset state
    spi_rx_state = SPI_RX_STATE_HEADER;
    spi_mosi_dma(spi_buf_rx, 7);
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
  spi_rx_state = SPI_RX_STATE_HEADER;
  spi_mosi_dma(spi_buf_rx, 7);

  NVIC_EnableIRQ(DMA2_Stream2_IRQn);
  NVIC_EnableIRQ(DMA2_Stream3_IRQn);
}
