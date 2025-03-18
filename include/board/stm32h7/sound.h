#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "board/boards/board.h"

extern board *current_board;

void register_set(volatile uint32_t *addr, uint32_t val, uint32_t mask);
void register_set_bits(volatile uint32_t *addr, uint32_t bits);
void register_clear_bits(volatile uint32_t *addr, uint32_t bits);

void sound_init(void);
void sound_tick(void);