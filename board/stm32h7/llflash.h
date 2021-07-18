bool flash_is_locked(void) {
  return (FLASH->CR1 & FLASH_CR_LOCK);
}

void flash_unlock(void) {
  FLASH->KEYR1 = 0x45670123;
  FLASH->KEYR1 = 0xCDEF89AB;
  FLASH->CR1 |= FLASH_CR_PG;
}

bool flash_erase_sector(uint8_t sector, bool unlocked) {
  // don't erase the bootloader(sector 0)
  if (sector != 0 && sector < 8 && unlocked) {
    FLASH->CR1 = (sector << 8) | FLASH_CR_SER;
    FLASH->CR1 |= FLASH_CR_START;
    while (FLASH->SR1 & FLASH_SR_BSY);
    return true;
  }
  return false;
}

void flash_write_word(void *prog_ptr, uint32_t data) {
  uint32_t *pp = prog_ptr;
  FLASH->CR1 = FLASH_CR_PSIZE_1 | FLASH_CR_PG;
  *pp = data;
  while (FLASH->SR1 & FLASH_SR_BSY);
}
