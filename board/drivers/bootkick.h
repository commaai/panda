#pragma once

#include "board/drivers/drivers.h"

extern bool bootkick_reset_triggered;

void bootkick_tick(bool ignition, bool recent_heartbeat);
