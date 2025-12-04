#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "board/board_struct.h"

// ******************** Prototypes ********************


void pwm_init(TIM_TypeDef *TIM, uint8_t channel);
void pwm_set(TIM_TypeDef *TIM, uint8_t channel, uint8_t percentage);

// ********************* Globals **********************
#include "globals.h"


// heartbeat state
extern uint32_t heartbeat_counter;
extern bool heartbeat_lost;
extern bool heartbeat_disabled;

// siren state
extern bool siren_enabled;
