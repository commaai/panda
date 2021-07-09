void flash_unlock(void) {
  if (FLASH->CR & FLASH_CR_LOCK) {
    FLASH->KEYR = 0x45670123;
    FLASH->KEYR = 0xCDEF89AB;
  }
}

void flash_erase_sector(uint8_t sector, uint8_t unlocked) {
  // don't erase the bootloader(sector 0)
  if (sector != 0 && sector < 12 && unlocked) {
    FLASH->CR = (sector << 3) | FLASH_CR_SER;
    FLASH->CR |= FLASH_CR_STRT;
    while (FLASH->SR & FLASH_SR_BSY);
  }
}

void flash_write_byte(void *prog_ptr, const void *data) {
  uint32_t *pp = prog_ptr;
  const uint32_t *d = data;
    FLASH->CR = FLASH_CR_PSIZE_1 | FLASH_CR_PG;
    *pp = *d;
    while (FLASH->SR & FLASH_SR_BSY);
}
