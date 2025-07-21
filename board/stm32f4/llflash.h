#pragma once

static inline bool flash_is_locked(void) {
  return (FLASH->CR & FLASH_CR_LOCK);
}

static inline void flash_unlock(void) {
  FLASH->KEYR = 0x45670123;
  FLASH->KEYR = 0xCDEF89AB;
}

static inline bool flash_erase_sector(uint8_t sector, bool unlocked) {
  // don't erase the bootloader(sector 0)
  if (sector != 0 && sector < 12 && unlocked) {
    FLASH->CR = (sector << 3) | FLASH_CR_SER;
    FLASH->CR |= FLASH_CR_STRT;
    while (FLASH->SR & FLASH_SR_BSY);
    return true;
  }
  return false;
}

static inline void flash_write_word(void *prog_ptr, uint32_t data) {
  uint32_t *pp = prog_ptr;
  FLASH->CR = FLASH_CR_PSIZE_1 | FLASH_CR_PG;
  *pp = data;
  while (FLASH->SR & FLASH_SR_BSY);
}

static inline void flush_write_buffer(void) { }
