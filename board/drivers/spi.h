#pragma once

//#define SPI_BUF_SIZE 1024U
#define SPI_BUF_SIZE 0x40U

#ifdef STM32H7
__attribute__((section(".ram_d1"))) uint8_t spi_buf_rx[SPI_BUF_SIZE];
__attribute__((section(".ram_d1"))) uint8_t spi_buf_tx[SPI_BUF_SIZE];
#else
uint8_t spi_buf_rx[SPI_BUF_SIZE];
uint8_t spi_buf_tx[SPI_BUF_SIZE];
#endif

#define SPI_CHECKSUM_START 0xABU
#define SPI_SYNC_BYTE 0x5AU
#define SPI_HACK 0x79U
#define SPI_DACK 0x85U
#define SPI_NACK 0x1FU

// SPI states
uint8_t spi_endpoint;
uint16_t spi_data_len_mosi;
uint16_t spi_data_len_miso;

#define SPI_HEADER_SIZE 6U

// low level SPI prototypes
void llspi_init(void);
void llspi_mosi_dma(uint8_t *addr, int len);
void llspi_miso_dma(uint8_t *addr, int len);

void spi_init(void) {
  // clear buffers (for debugging)
  (void)memset(spi_buf_rx, 0, SPI_BUF_SIZE);
  (void)memset(spi_buf_tx, 0, SPI_BUF_SIZE);

  // platform init
  llspi_init();

  // Start the first packet!
  llspi_mosi_dma(spi_buf_rx, SPI_BUF_SIZE);
}

bool check_checksum(uint8_t *data, uint16_t len) {
  // TODO: can speed this up by casting the bulk to uint32_t and xor-ing the bytes afterwards
  uint8_t checksum = SPI_CHECKSUM_START;
  for(uint16_t i = 0U; i < len; i++){
    checksum ^= data[i];
  }
  return checksum == 0U;
}

void spi_handle_rx(void) {
  // TODO: use the struct for this?
  // parse header
  spi_endpoint = spi_buf_rx[1];
  spi_data_len_mosi = (spi_buf_rx[3] << 8) | spi_buf_rx[2];
  //spi_data_len_miso = (spi_buf_rx[5] << 8) | spi_buf_rx[4];

  // for debugging
  (void)memset(spi_buf_tx, 99, SPI_BUF_SIZE);

  bool response_ack = false;
  uint16_t response_len = 0U;

  if (spi_buf_rx[0] == SPI_SYNC_BYTE) {
    //print("- all good, got header: "); hexdump(spi_buf_rx, SPI_HEADER_SIZE);

    if (check_checksum(spi_buf_rx, SPI_HEADER_SIZE + spi_data_len_mosi + 1U)) {
      if (spi_endpoint == 0U) {
        if (spi_data_len_mosi == sizeof(ControlPacket_t)) {
          ControlPacket_t ctrl;
          (void)memcpy(&ctrl, &spi_buf_rx[SPI_HEADER_SIZE], sizeof(ControlPacket_t));
          response_len = comms_control_handler(&ctrl, &spi_buf_tx[3]);
          response_ack = true;
        } else {
          print("SPI: wrong length of data for control handler, got "); puth(spi_data_len_mosi); print(", expected "); puth(sizeof(ControlPacket_t)); print("\n");
        }
      } else {
        print("- unknown endpoint "); puth(spi_endpoint); print("\n");
      }
    } else {
      print("- incorrect checksum "); hexdump(spi_buf_rx, SPI_HEADER_SIZE + spi_data_len_mosi);
    }
  } else {
    print("- incorrect header sync "); hexdump(spi_buf_rx, SPI_HEADER_SIZE);
  }

  if (spi_buf_rx[0] != 0x00) {
    // Setup response header
    spi_buf_tx[0] = response_ack ? SPI_DACK : SPI_NACK;
    spi_buf_tx[1] = response_len & 0xFFU;
    spi_buf_tx[2] = (response_len >> 8) & 0xFFU;

    // Add checksum
    uint8_t checksum = SPI_CHECKSUM_START;
    for (uint16_t i = 0U; i < (response_len + 3U); i++) {
      checksum ^= spi_buf_tx[i];
    }
    spi_buf_tx[response_len + 3U] = checksum;

    // send off the response
    llspi_miso_dma(spi_buf_tx, SPI_BUF_SIZE);
  } else {
    print("- skip TX\n");
    llspi_mosi_dma(spi_buf_rx, SPI_BUF_SIZE);
  }
}

void spi_handle_tx(void) {
  // for debugging
  (void)memset(spi_buf_rx, 99, SPI_BUF_SIZE);

  // back to reading
  llspi_mosi_dma(spi_buf_rx, SPI_BUF_SIZE);
}
