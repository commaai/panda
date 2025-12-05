#include "llflash.h"

#include "stm32h7_config.h"

// cppcheck-suppress misra-c2012-8.7
bool flash_is_locked(void) {
  return (FLASH->CR1 & FLASH_CR_LOCK);
}

// cppcheck-suppress misra-c2012-8.7
void flash_unlock(void) {
  FLASH->KEYR1 = 0x45670123u;
  FLASH->KEYR1 = 0xCDEF89ABu;
}

// cppcheck-suppress misra-c2012-8.7
bool flash_erase_sector(uint8_t sector, bool is_unlocked) {
  // don't erase the bootloader(sector 0)
  bool ret = false;
  if ((sector != 0u) && (sector < 8u) && is_unlocked) {
    FLASH->CR1 = ((uint32_t) sector << 8u) | FLASH_CR_SER;
    FLASH->CR1 |= FLASH_CR_START;
    while ((FLASH->SR1 & FLASH_SR_QW) != 0u) {}
    ret = true;
  }
  return ret;
}

// cppcheck-suppress misra-c2012-8.7
void flash_write_word(void *prog_ptr, uint32_t data) {
  uint32_t *pp = prog_ptr;
  FLASH->CR1 |= FLASH_CR_PG;
  *pp = data;
  while ((FLASH->SR1 & FLASH_SR_QW) != 0u) {}
}

// cppcheck-suppress misra-c2012-8.7
void flush_write_buffer(void) {
  if ((FLASH->SR1 & FLASH_SR_WBNE) != 0u) {
    FLASH->CR1 |= FLASH_CR_FW;
    while ((FLASH->SR1 & FLASH_CR_FW) != 0u) {}
  }
}
