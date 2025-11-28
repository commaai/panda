#pragma once

#include <stdint.h>

#include "board/body/motor_common.h"

// Encoder pin map:
// Left motor:  PB6 -> TIM4_CH1, PB7 -> TIM4_CH2
// Right motor: PA6 -> TIM3_CH1, PA7 -> TIM3_CH2

typedef struct {
  TIM_TypeDef *timer;
  GPIO_TypeDef *pin_a_port;
  uint8_t pin_a;
  uint8_t pin_a_af;
  GPIO_TypeDef *pin_b_port;
  uint8_t pin_b;
  uint8_t pin_b_af;
  int8_t direction;
  uint32_t counts_per_output_rev;
  uint32_t min_dt_us;
  float speed_alpha;
  uint32_t filter;
} motor_encoder_config_t;

typedef struct {
  const motor_encoder_config_t *config;
  uint16_t last_timer_count;
  int32_t accumulated_count;
  int32_t last_speed_count;
  uint32_t last_speed_timestamp_us;
  float cached_speed_rps;
} motor_encoder_state_t;

static const motor_encoder_config_t motor_encoder_config[BODY_MOTOR_COUNT] = {
  [BODY_MOTOR_LEFT - 1U] = {
    .timer = TIM4,
    .pin_a_port = GPIOB, .pin_a = 6U, .pin_a_af = GPIO_AF2_TIM4,
    .pin_b_port = GPIOB, .pin_b = 7U, .pin_b_af = GPIO_AF2_TIM4,
    .direction = -1,
    .counts_per_output_rev = 44U * 90U,
    .min_dt_us = 250U,
    .speed_alpha = 0.2f,
    .filter = 3U,
  },
  [BODY_MOTOR_RIGHT - 1U] = {
    .timer = TIM3,
    .pin_a_port = GPIOA, .pin_a = 6U, .pin_a_af = GPIO_AF2_TIM3,
    .pin_b_port = GPIOA, .pin_b = 7U, .pin_b_af = GPIO_AF2_TIM3,
    .direction = 1,
    .counts_per_output_rev = 44U * 90U,
    .min_dt_us = 250U,
    .speed_alpha = 0.2f,
    .filter = 3U,
  },
};

static motor_encoder_state_t motor_encoders[BODY_MOTOR_COUNT] = {
  { .config = &motor_encoder_config[0] },
  { .config = &motor_encoder_config[1] },
};

