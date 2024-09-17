#pragma once

#include <stdint.h>
#include <stdbool.h>

extern bool bootkick_reset_triggered;

void bootkick_tick(bool ignition, bool recent_heartbeat);
