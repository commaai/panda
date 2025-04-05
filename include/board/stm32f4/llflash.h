#pragma once
#include <stdint.h>
#include <stdbool.h>

bool flash_is_locked(void);
void flash_unlock(void);
bool flash_erase_sector(uint8_t sector, bool unlocked);
void flash_write_word(void *prog_ptr, uint32_t data);
void flush_write_buffer(void);
