#pragma once

#include "board/drivers/drivers.h"

void clock_source_set_timer_params(uint16_t param1, uint16_t param2);
void clock_source_init(bool enable_channel1);