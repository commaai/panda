bool flash_is_locked(void) {
  //return (FLASH->CR & FLASH_CR_LOCK);
  return false;
}

void flash_unlock(void) {
  //FLASH->KEYR = 0x45670123;
  //FLASH->KEYR = 0xCDEF89AB;
  //FLASH->CR |= FLASH_CR_PG;
}

bool flash_erase_sector(uint8_t sector, bool unlocked) {
  UNUSED(sector);
  UNUSED(unlocked);
  return false;
}

void flash_write_word(void *prog_ptr, uint32_t data) {
  UNUSED(prog_ptr);
  UNUSED(data);
}
