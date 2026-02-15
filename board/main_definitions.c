#include <stdbool.h>
#include <stdint.h>

#include "globals.h"

// heartbeat state
uint32_t heartbeat_counter = 0;
bool heartbeat_lost = false;
bool heartbeat_disabled = false;            // set over USB

// siren state
bool siren_enabled = false;
