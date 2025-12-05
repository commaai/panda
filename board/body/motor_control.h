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

void motor_speed_controller_init(void);
void motor_init(void);
void motor_set_speed(uint8_t motor, int8_t speed);
void motor_speed_controller_set_target_rpm(uint8_t motor, float target_rpm);
void motor_speed_controller_update(uint32_t now_us);
