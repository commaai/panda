#ifndef PANDA_DRIVERS_CLOCK_SOURCE_H
#define PANDA_DRIVERS_CLOCK_SOURCE_H

#include <stdbool.h>
#include <stdint.h>

void clock_source_set_timer_params(uint16_t param1, uint16_t param2);
void clock_source_init(bool enable_channel1);

#endif
