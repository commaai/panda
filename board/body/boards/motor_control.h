#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "board/body/boards/motor_encoder.h"

// Motor pin map:
// M1 drive: PB8 -> TIM16_CH1, PB9 -> TIM17_CH1, PE2/PE3 enables
// M2 drive: PA2 -> TIM15_CH1, PA3 -> TIM15_CH2, PE8/PE9 enables

// TIM15/16/17 are on APB2 running at 120 MHz on the body board
#define MOTOR_PWM_TIMER_CLOCK_HZ 120000000U
#define MOTOR_PWM_FREQUENCY_HZ 5000U

#define MOTOR_SPEED_CONTROL_KP 0.25f
#define MOTOR_SPEED_CONTROL_KI 0.5f
#define MOTOR_SPEED_CONTROL_KD 0.008f
#define MOTOR_SPEED_CONTROL_KFF 0.9f

#define MOTOR_SPEED_CONTROL_TARGET_MAX_RPM 100.0f
#define MOTOR_SPEED_CONTROL_OUTPUT_MAX 100.0f
#define MOTOR_SPEED_CONTROL_DEADBAND_RPM 1.0f

#define MOTOR_SPEED_CONTROL_INTEGRAL_LIMIT 50.0f
#define MOTOR_SPEED_CONTROL_DERIVATIVE_MAX_RPMS 250.0f

#define MOTOR_SPEED_CONTROL_UPDATE_PERIOD_US 1000U

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

static motor_speed_state_t motor_speed_states[2];

static inline motor_speed_state_t *motor_speed_state_get(uint8_t motor) {
  if ((motor == 0U) || (motor > 2U)) {
    return NULL;
  }
  return &motor_speed_states[motor - 1U];
}

static inline void motor_speed_state_reset(motor_speed_state_t *state) {
  state->active = false;
  state->target_rpm = 0.0f;
  state->integral = 0.0f;
  state->previous_error = 0.0f;
  state->last_output = 0.0f;
  state->last_update_us = 0U;
}

static inline uint16_t motor_pwm_reload_value(void) {
  uint32_t clock_hz = MOTOR_PWM_TIMER_CLOCK_HZ;
  uint32_t target_hz = MOTOR_PWM_FREQUENCY_HZ;
  if (target_hz == 0U) {
    target_hz = 1U;
  }

  // Round to the nearest integer number of timer ticks per PWM period
  uint32_t ticks_per_period = (clock_hz + (target_hz / 2U)) / target_hz;
  if (ticks_per_period == 0U) {
    ticks_per_period = 1U;
  } else if (ticks_per_period > 0xFFFFU) {
    ticks_per_period = 0xFFFFU;
  }

  return (uint16_t)ticks_per_period;
}

static inline void motor_pwm_apply_frequency(TIM_TypeDef *TIM) {
  uint16_t reload = motor_pwm_reload_value();
  register_set(&(TIM->PSC), 0U, 0xFFFFU);
  register_set(&(TIM->ARR), (uint32_t)reload, 0xFFFFU);
  TIM->EGR |= TIM_EGR_UG;
}

static void motor_speed_controller_init(void) {
  for (uint8_t i = 0U; i < 2U; i++) {
    motor_speed_state_reset(&motor_speed_states[i]);
  }
}

static void motor_speed_controller_disable(uint8_t motor) {
  motor_speed_state_t *state = motor_speed_state_get(motor);
  if (state == NULL) {
    return;
  }
  motor_speed_state_reset(state);
}

static void motor_init(void) {
  register_set_bits(&(RCC->AHB4ENR), RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOBEN | RCC_AHB4ENR_GPIOEEN);
  register_set_bits(&(RCC->APB2ENR), RCC_APB2ENR_TIM16EN | RCC_APB2ENR_TIM17EN | RCC_APB2ENR_TIM15EN);

  set_gpio_output(GPIOE, 2U, 0);
  set_gpio_output(GPIOE, 3U, 0);
  set_gpio_output(GPIOE, 8U, 0);
  set_gpio_output(GPIOE, 9U, 0);

  set_gpio_alternate(GPIOB, 8U, GPIO_AF1_TIM16);
  set_gpio_alternate(GPIOB, 9U, GPIO_AF1_TIM17);
  set_gpio_alternate(GPIOA, 2U, GPIO_AF4_TIM15);
  set_gpio_alternate(GPIOA, 3U, GPIO_AF4_TIM15);

  pwm_init(TIM16, 1);
  motor_pwm_apply_frequency(TIM16);
  pwm_init(TIM17, 1);
  motor_pwm_apply_frequency(TIM17);
  pwm_init(TIM15, 1);
  pwm_init(TIM15, 2);
  motor_pwm_apply_frequency(TIM15);

  // Advanced timers require MOE to drive outputs
  register_set(&(TIM15->BDTR), TIM_BDTR_MOE, 0xFFFFU);
  register_set(&(TIM16->BDTR), TIM_BDTR_MOE, 0xFFFFU);
  register_set(&(TIM17->BDTR), TIM_BDTR_MOE, 0xFFFFU);
}

static void motor_apply_pwm(uint8_t motor, int32_t speed_command) {
  if ((motor == 0U) || (motor > 2U)) {
    return;
  }

  if (speed_command > 100) {
    speed_command = 100;
  } else if (speed_command < -100) {
    speed_command = -100;
  }

  int8_t speed = (int8_t)speed_command;
  uint8_t pwm_value = (uint8_t)((speed < 0) ? -speed : speed);

  if (motor == 1U) {
    if (speed > 0) {
      pwm_set(TIM16, 1, pwm_value);
      pwm_set(TIM17, 1, 0);
      set_gpio_output(GPIOE, 2U, 1);
      set_gpio_output(GPIOE, 3U, 1);
    } else if (speed < 0) {
      pwm_set(TIM16, 1, 0);
      pwm_set(TIM17, 1, pwm_value);
      set_gpio_output(GPIOE, 2U, 1);
      set_gpio_output(GPIOE, 3U, 1);
    } else {
      pwm_set(TIM16, 1, 0);
      pwm_set(TIM17, 1, 0);
      set_gpio_output(GPIOE, 2U, 0);
      set_gpio_output(GPIOE, 3U, 0);
    }
  } else {
    if (speed > 0) {
      pwm_set(TIM15, 2, pwm_value);
      pwm_set(TIM15, 1, 0);
      set_gpio_output(GPIOE, 8U, 1);
      set_gpio_output(GPIOE, 9U, 1);
    } else if (speed < 0) {
      pwm_set(TIM15, 2, 0);
      pwm_set(TIM15, 1, pwm_value);
      set_gpio_output(GPIOE, 8U, 1);
      set_gpio_output(GPIOE, 9U, 1);
    } else {
      pwm_set(TIM15, 1, 0);
      pwm_set(TIM15, 2, 0);
      set_gpio_output(GPIOE, 8U, 0);
      set_gpio_output(GPIOE, 9U, 0);
    }
  }
}

static inline void motor_set_speed(uint8_t motor, int8_t speed) {
  motor_speed_controller_disable(motor);
  motor_apply_pwm(motor, (int32_t)speed);
}

static inline void motor_speed_controller_set_target_rpm(uint8_t motor, float target_rpm) {
  motor_speed_state_t *state = motor_speed_state_get(motor);
  if (state == NULL) {
    return;
  }

  target_rpm = motor_clampf(target_rpm, -MOTOR_SPEED_CONTROL_TARGET_MAX_RPM, MOTOR_SPEED_CONTROL_TARGET_MAX_RPM);
  if (motor_absf(target_rpm) <= MOTOR_SPEED_CONTROL_DEADBAND_RPM) {
    motor_speed_controller_disable(motor);
    motor_apply_pwm(motor, 0);
    return;
  }

  state->active = true;
  state->target_rpm = target_rpm;
  state->integral = 0.0f;
  state->previous_error = target_rpm - motor_encoder_get_speed_rpm(motor);
  state->last_output = 0.0f;
  state->last_update_us = 0U;
}

static inline void motor_speed_controller_update(uint32_t now_us) {
  for (uint8_t motor = 1U; motor <= 2U; motor++) {
    motor_speed_state_t *state = motor_speed_state_get(motor);
    if ((state == NULL) || !state->active) {
      continue;
    }

    bool first_update = (state->last_update_us == 0U);
    uint32_t dt_us = first_update ? MOTOR_SPEED_CONTROL_UPDATE_PERIOD_US : (now_us - state->last_update_us);
    if (!first_update && (dt_us < MOTOR_SPEED_CONTROL_UPDATE_PERIOD_US)) {
      continue;
    }

    float measured_rpm = motor_encoder_get_speed_rpm(motor);
    float error = state->target_rpm - measured_rpm;

    if ((motor_absf(state->target_rpm) <= MOTOR_SPEED_CONTROL_DEADBAND_RPM) &&
        (motor_absf(error) <= MOTOR_SPEED_CONTROL_DEADBAND_RPM) &&
        (motor_absf(measured_rpm) <= MOTOR_SPEED_CONTROL_DEADBAND_RPM)) {
      motor_speed_controller_disable(motor);
      motor_apply_pwm(motor, 0);
      continue;
    }

    float dt_s = (float)dt_us * 1.0e-6f;
    float control = MOTOR_SPEED_CONTROL_KFF * state->target_rpm;

    if (dt_s > 0.0f) {
      state->integral += error * dt_s;
      state->integral = motor_clampf(state->integral, -MOTOR_SPEED_CONTROL_INTEGRAL_LIMIT, MOTOR_SPEED_CONTROL_INTEGRAL_LIMIT);

      float derivative = 0.0f;
      if (!first_update) {
        derivative = (error - state->previous_error) / dt_s;
        derivative = motor_clampf(derivative, -MOTOR_SPEED_CONTROL_DERIVATIVE_MAX_RPMS, MOTOR_SPEED_CONTROL_DERIVATIVE_MAX_RPMS);
      }

      control += (MOTOR_SPEED_CONTROL_KP * error) +
                 (MOTOR_SPEED_CONTROL_KI * state->integral) +
                 (MOTOR_SPEED_CONTROL_KD * derivative);
    } else {
      state->integral = 0.0f;
      control += MOTOR_SPEED_CONTROL_KP * error;
    }

    if ((state->target_rpm > 0.0f) && (control < 0.0f)) {
      control = 0.0f;
      state->integral = 0.0f;
    } else if ((state->target_rpm < 0.0f) && (control > 0.0f)) {
      control = 0.0f;
      state->integral = 0.0f;
    }

    control = motor_clampf(control, -MOTOR_SPEED_CONTROL_OUTPUT_MAX, MOTOR_SPEED_CONTROL_OUTPUT_MAX);

    int32_t command = (control >= 0.0f) ? (int32_t)(control + 0.5f) : (int32_t)(control - 0.5f);
    motor_apply_pwm(motor, command);

    state->previous_error = error;
    state->last_output = control;
    state->last_update_us = now_us;
  }
}
