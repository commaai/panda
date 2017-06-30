#include "libc.h"

/*void lock_bootloader() {
  if (FLASH->OPTCR & FLASH_OPTCR_nWRP_0) {
    FLASH->OPTKEYR = 0x08192A3B;
    FLASH->OPTKEYR = 0x4C5D6E7F;

    // write protect the bootloader
    FLASH->OPTCR &= ~FLASH_OPTCR_nWRP_0;

    // OPT program
    FLASH->OPTCR |= FLASH_OPTCR_OPTSTRT;
    while (FLASH->SR & FLASH_SR_BSY);

    // relock it
    FLASH->OPTCR |= FLASH_OPTCR_OPTLOCK;

    // reset
    NVIC_SystemReset();
  }
}*/

void spi_flasher() {
  // green LED on for flashing
  GPIOC->MODER |= GPIO_MODER_MODER6_0;
  GPIOC->ODR &= ~(1 << 6);

  RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
  RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

  // setup SPI
  GPIOA->MODER = GPIO_MODER_MODER4_1 | GPIO_MODER_MODER5_1 |
                 GPIO_MODER_MODER6_1 | GPIO_MODER_MODER7_1;
  GPIOA->AFR[0] = GPIO_AF5_SPI1 << (4*4) | GPIO_AF5_SPI1 << (5*4) |
                  GPIO_AF5_SPI1 << (6*4) | GPIO_AF5_SPI1 << (7*4);

  // flasher
  spi_init();

  char spi_rx_buf[0x14];
  char spi_tx_buf[0x44];

  int i;
  int sec;
  int rcv = 0;
  int lastval = (GPIOA->IDR & (1 << 4));
  uint32_t *prog_ptr = (uint32_t *)0x8004000;

  while (1) {
    int val = (GPIOA->IDR & (1 << 4));
    if (!val && lastval) {
      spi_rx_dma(spi_rx_buf, 0x14);
      rcv = 1;
    }
    lastval = val;

    if (rcv && (DMA2->LISR & DMA_LISR_TCIF2)) {
      DMA2->LIFCR = DMA_LIFCR_CTCIF2;

      rcv = 0;
      memset(spi_tx_buf, 0, 0x44);
      spi_tx_buf[0x40] = 0xde;
      spi_tx_buf[0x41] = 0xad;
      spi_tx_buf[0x42] = 0xd0;
      spi_tx_buf[0x43] = 0x0d;

      if (memcmp("\x00\x00\x00\x00\x40\xD0\x00\x00\x00\x00\x20\x00", spi_rx_buf, 0xC) == 0) {
        *(uint32_t*)(&spi_tx_buf[0]) = 0x20;
        memcpy(spi_tx_buf+4, (void *)0x1fff79e0, 0x20);
      } else if (spi_rx_buf[0] == (0xff^spi_rx_buf[1]) &&
                 spi_rx_buf[2] == (0xff^spi_rx_buf[3])) {
        spi_tx_buf[0] = 0xff;
        *(uint32_t*)(&spi_tx_buf[4]) = FLASH->CR;
        *(uint32_t*)(&spi_tx_buf[8]) = (uint32_t)prog_ptr;
        // valid
        switch (spi_rx_buf[0]) {
          case 0x10:
            // unlock flash
            if (FLASH->CR & FLASH_CR_LOCK) {
              FLASH->KEYR = 0x45670123;
              FLASH->KEYR = 0xCDEF89AB;
              spi_tx_buf[1] = 0xff;
            }
            break;
          case 0x11:
            // erase
            sec = spi_rx_buf[2] & 0xF;
            // don't erase the bootloader
            if (sec != 0 && sec < 12) {
              FLASH->CR = (sec << 3) | FLASH_CR_SER;
              FLASH->CR |= FLASH_CR_STRT;
              while (FLASH->SR & FLASH_SR_BSY);
              spi_tx_buf[1] = 0xff;
            }
            break;
          case 0x12:
            if (spi_rx_buf[2] <= 4) {
              for (i = 0; i < spi_rx_buf[2]; i++) {
                // program byte 1
                FLASH->CR = FLASH_CR_PSIZE_1 | FLASH_CR_PG;

                *prog_ptr = *(uint32_t*)(spi_rx_buf+4+(i*4));
                while (FLASH->SR & FLASH_SR_BSY);

                *(uint64_t*)(&spi_tx_buf[0x30+(i*4)]) = *prog_ptr;
                prog_ptr++;
              }
              spi_tx_buf[1] = 0xff;
            }
            break;
          case 0x13:
            // reset
            NVIC_SystemReset();
            break;
          case 0x14:
            // bootloader
            /*enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
            NVIC_SystemReset();*/
            break;
          default:
            break;
        }
        memcpy(spi_tx_buf+0x10, spi_rx_buf, 0x14);
      }

      spi_tx_dma(spi_tx_buf, 0x44);

      // signal data is ready by driving low
      // esp must be configured as input by this point
      GPIOB->MODER &= ~(GPIO_MODER_MODER0);
      GPIOB->MODER |= GPIO_MODER_MODER0_0;
      GPIOB->ODR &= ~(GPIO_ODR_ODR_0);
    }

    if (DMA2->LISR & DMA_LISR_TCIF3) {
      DMA2->LIFCR = DMA_LIFCR_CTCIF3;

      // reset handshake back to pull up
      GPIOB->MODER &= ~(GPIO_MODER_MODER0);
      GPIOB->PUPDR |= GPIO_PUPDR_PUPDR0_0;
    }
  }
}
