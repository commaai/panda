#pragma once

#include <stdint.h>
#include <stdbool.h>

// heartbeat state
extern uint32_t heartbeat_counter;
extern bool heartbeat_lost;
extern bool heartbeat_disabled;            // set over USB

 // siren state
extern bool siren_enabled;
