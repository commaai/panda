#pragma once

#include "board/drivers/drivers.h"

// ******************** Prototypes ********************
typedef struct board board;
typedef struct harness_configuration harness_configuration;

void set_safety_mode(uint16_t mode, uint16_t param);
bool is_car_safety_mode(uint16_t mode);

// ********************* Globals **********************
extern uint8_t hw_type;
extern board *current_board;
extern uint32_t uptime_cnt;

// heartbeat state
extern uint32_t heartbeat_counter;
extern bool heartbeat_lost;
extern bool heartbeat_disabled;

// siren state
extern bool siren_enabled;

// sound
extern uint16_t sound_output_level;
