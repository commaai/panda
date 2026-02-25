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

void pwm_init(TIM_TypeDef *TIM, uint8_t channel);
void pwm_set(TIM_TypeDef *TIM, uint8_t channel, uint8_t percentage);
