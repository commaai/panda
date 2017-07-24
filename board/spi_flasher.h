// can't go on the stack cause it's DMAed
uint8_t spi_tx_buf[0x44];

uint32_t *prog_ptr;
int unlocked = 0;

int spi_cb_rx(uint8_t *data, int len, uint8_t *data_out) {
  // get serial number even in bootstub mode
  if (memcmp("\x00\x00\x00\x00\x40\xD0\x00\x00\x00\x00\x20\x00", data, 0xC) == 0) {
    memcpy(data_out, (void *)0x1fff79e0, 0x20);
    return 0x20;
  }

  // flasher mode
  if (data[0] == (0xff^data[1]) &&
      data[2] == (0xff^data[3])) {
    int sec;
    memset(data_out, 0, 4);
    memcpy(data_out+4, "\xde\xad\xd0\x0d", 4);
    data_out[0] = 0xff;
    data_out[2] = data[0];
    data_out[3] = data[1];
    switch (data[0]) {
      case 0xf:
        // echo
        data_out[1] = 0xff;
        break;
      case 0x10:
        // unlock flash
        if (FLASH->CR & FLASH_CR_LOCK) {
          FLASH->KEYR = 0x45670123;
          FLASH->KEYR = 0xCDEF89AB;
          data_out[1] = 0xff;
        }
        set_led(LED_GREEN, 1);
        unlocked = 1;
        prog_ptr = (uint32_t *)0x8004000;
        break;
      case 0x11:
        // erase
        sec = data[2] & 0xF;
        // don't erase the bootloader
        if (sec != 0 && sec < 12 && unlocked) {
          FLASH->CR = (sec << 3) | FLASH_CR_SER;
          FLASH->CR |= FLASH_CR_STRT;
          while (FLASH->SR & FLASH_SR_BSY);
          data_out[1] = 0xff;
        }
        break;
      case 0x12:
        if (data[2] <= 4 && unlocked) {
          set_led(LED_RED, 0);
          for (int i = 0; i < data[2]; i++) {
            // program byte 1
            FLASH->CR = FLASH_CR_PSIZE_1 | FLASH_CR_PG;

            *prog_ptr = *(uint32_t*)(data+4+(i*4));
            while (FLASH->SR & FLASH_SR_BSY);

            //*(uint64_t*)(&spi_tx_buf[0x30+(i*4)]) = *prog_ptr;
            prog_ptr++;
          }
          set_led(LED_RED, 1);
          data_out[1] = 0xff;
        }
        break;
      case 0x13:
        // reset
        NVIC_SystemReset();
        break;
      default:
        break;
    }
  }
  
  return 8;
}

void spi_flasher() {
  __disable_irq();
  
  RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
  RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

  // setup SPI
  GPIOA->MODER = GPIO_MODER_MODER4_1 | GPIO_MODER_MODER5_1 |
                 GPIO_MODER_MODER6_1 | GPIO_MODER_MODER7_1;
  GPIOA->AFR[0] = GPIO_AF5_SPI1 << (4*4) | GPIO_AF5_SPI1 << (5*4) |
                  GPIO_AF5_SPI1 << (6*4) | GPIO_AF5_SPI1 << (7*4);

  // blue LED on for flashing
  set_led(LED_BLUE, 1);

  // flasher
  spi_init();
  __enable_irq();

  while (1) { }
}

