#pragma once

#include <stdint.h>
#include <stdbool.h>

// External variables
extern bool bootkick_reset_triggered;

// Function declarations
void bootkick_tick(bool ignition, bool recent_heartbeat);
