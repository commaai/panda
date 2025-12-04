#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "board/body/motor_common.h"
#include "board/body/motor_encoder.h"

#include "board/drivers/pwm.h"

// Motor pin map:
// M1 drive: PB8 -> TIM16_CH1, PB9 -> TIM17_CH1, PE2/PE3 enables
// M2 drive: PA2 -> TIM15_CH1, PA3 -> TIM15_CH2, PE8/PE9 enables

#define PWM_TIMER_CLOCK_HZ 120000000U
#define PWM_FREQUENCY_HZ 5000U
#define PWM_PERCENT_MAX 100
#define PWM_RELOAD_TICKS ((PWM_TIMER_CLOCK_HZ + (PWM_FREQUENCY_HZ / 2U)) / PWM_FREQUENCY_HZ)

#define KP         0.25f
#define KI         0.5f
#define KD         0.008f
#define KFF        0.9f
#define MAX_RPM    100.0f
#define OUTPUT_MAX 100.0f
#define DEADBAND_RPM 1.0f
#define UPDATE_PERIOD_US 1000U

typedef struct {
  TIM_TypeDef *forward_timer;
  uint8_t forward_channel;
  TIM_TypeDef *reverse_timer;
  uint8_t reverse_channel;
  GPIO_TypeDef *pwm_port[2];
  uint8_t pwm_pin[2];
  uint8_t pwm_af[2];
  GPIO_TypeDef *enable_port[2];
  uint8_t enable_pin[2];
} motor_pwm_config_t;

static const motor_pwm_config_t motor_pwm_config[BODY_MOTOR_COUNT] = {
  [BODY_MOTOR_LEFT - 1U] = {
    TIM16, 1U, TIM17, 1U,
    {GPIOB, GPIOB}, {8U, 9U}, {GPIO_AF1_TIM16, GPIO_AF1_TIM17},
    {GPIOE, GPIOE}, {2U, 3U},
  },
  [BODY_MOTOR_RIGHT - 1U] = {
    TIM15, 2U, TIM15, 1U,
    {GPIOA, GPIOA}, {2U, 3U}, {GPIO_AF4_TIM15, GPIO_AF4_TIM15},
    {GPIOE, GPIOE}, {8U, 9U},
  },
};

typedef struct {
  bool active;
  float target_rpm;
  float integral;
  float previous_error;
  float last_output;
  uint32_t last_update_us;
} motor_speed_state_t;

static inline float motor_absf(float value) {
  return (value < 0.0f) ? -value : value;
}

static inline float motor_clampf(float value, float min_value, float max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

static motor_speed_state_t motor_speed_states[BODY_MOTOR_COUNT];

static inline void motor_pwm_write(TIM_TypeDef *timer, uint8_t channel, uint8_t percentage) {
  uint32_t period = (timer->ARR != 0U) ? timer->ARR : PWM_RELOAD_TICKS;
  uint16_t comp = (uint16_t)((period * (uint32_t)percentage) / 100U);
  if (channel == 1U) {
    register_set(&(timer->CCR1), comp, 0xFFFFU);
  } else if (channel == 2U) {
    register_set(&(timer->CCR2), comp, 0xFFFFU);
  }
}

static inline motor_speed_state_t *motor_speed_state_get(uint8_t motor) {
  return body_motor_is_valid(motor) ? &motor_speed_states[motor - 1U] : NULL;
}

static inline void motor_speed_state_reset(motor_speed_state_t *state) {
  state->active = false;
  state->target_rpm = 0.0f;
  state->integral = 0.0f;
  state->previous_error = 0.0f;
  state->last_output = 0.0f;
  state->last_update_us = 0U;
}

void motor_speed_controller_init(void);
void motor_speed_controller_disable(uint8_t motor);
void motor_init(void);
void motor_apply_pwm(uint8_t motor, int32_t speed_command);
void motor_set_speed(uint8_t motor, int8_t speed);
void motor_speed_controller_set_target_rpm(uint8_t motor, float target_rpm);
void motor_speed_controller_update(uint32_t now_us);
