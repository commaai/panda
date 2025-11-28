#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "boards/board_declarations.h"

// ******************** Prototypes ********************


void pwm_init(TIM_TypeDef *TIM, uint8_t channel);
void pwm_set(TIM_TypeDef *TIM, uint8_t channel, uint8_t percentage);

// ********************* Globals **********************
#include "globals.h"

extern uint32_t uptime_cnt;

// heartbeat state
extern uint32_t heartbeat_counter;
extern bool heartbeat_lost;
extern bool heartbeat_disabled;

// siren state
extern bool siren_enabled;
