#ifndef DRIVERS_BOOTKICK_H
#define DRIVERS_BOOTKICK_H

#include <stdbool.h>

extern bool bootkick_reset_triggered;

void bootkick_tick(bool ignition, bool recent_heartbeat);

#endif
