#pragma once
#include <stdint.h>
#include "board/boards/board.h"

extern board *current_board;

void adc_init(void);
uint16_t adc_get_mV(uint8_t channel);