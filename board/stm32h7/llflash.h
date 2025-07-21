static inline bool flash_is_locked(void) {
  return (FLASH->CR1 & FLASH_CR_LOCK);
}

static inline void flash_unlock(void) {
  FLASH->KEYR1 = 0x45670123;
  FLASH->KEYR1 = 0xCDEF89AB;
}

static inline bool flash_erase_sector(uint8_t sector, bool unlocked) {
  // don't erase the bootloader(sector 0)
  if (sector != 0 && sector < 8 && unlocked) {
    FLASH->CR1 = (sector << 8) | FLASH_CR_SER;
    FLASH->CR1 |= FLASH_CR_START;
    while (FLASH->SR1 & FLASH_SR_QW);
    return true;
  }
  return false;
}

static inline void flash_write_word(void *prog_ptr, uint32_t data) {
  uint32_t *pp = prog_ptr;
  FLASH->CR1 |= FLASH_CR_PG;
  *pp = data;
  while (FLASH->SR1 & FLASH_SR_QW);
}

static inline void flush_write_buffer(void) {
  if (FLASH->SR1 & FLASH_SR_WBNE) {
    FLASH->CR1 |= FLASH_CR_FW;
    while (FLASH->SR1 & FLASH_CR_FW);
  }
}
