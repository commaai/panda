#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct board board;
typedef struct harness_configuration harness_configuration;

extern uint8_t hw_type;
extern board *current_board;
extern uint32_t uptime_cnt;

extern uint32_t heartbeat_counter;
extern bool heartbeat_lost;
extern bool heartbeat_disabled;

extern bool siren_enabled;

// sound
extern uint16_t sound_output_level;
