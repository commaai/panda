#pragma once

#include "stm32h7xx.h"

#define COM_CTRL 0
#define SIN_CTRL 1
#define FOC_CTRL 2

#define OPEN_MODE 0
#define VLT_MODE 1
#define SPD_MODE 2
#define TRQ_MODE 3

#define DIAG_ENA 1

#define FIELD_WEAK_MAX 5
#define PHASE_ADV_MAX 25
#define FIELD_WEAK_HI 1000
#define FIELD_WEAK_LO 750

#define VOLTS_PER_PERCENT 0.00814

#define PWM_FREQ 32000
#define PWM_MARGIN 100
#define CF_SPEED_COEF (PWM_FREQ / 3)
#define MAX_RPM 1000
#define RPM_TO_UNIT 16
#define RPM_DEADBAND 1

#define BODY_MOTOR_LEFT 1U
#define BODY_MOTOR_RIGHT 2U

#define STALL_DEQUAL_TIME_MS 3000
#define T_ERR_DEQUAL_CYCLES (uint16_t)(STALL_DEQUAL_TIME_MS * (PWM_FREQ / 2) / 1000)

#define I_DC_MAX 6
#define I_MOT_MAX 6
#define A2BIT_CONV 310
#define N_MOT_MAX 1000

#define CTRL_TYP_SEL FOC_CTRL
#define CTRL_MOD_REQ SPD_MODE

#define FIELD_WEAK_ENA 1

#define BAT_CELLS 3
#define BAT_CELL_FULL_MV 4200U
#define BAT_CELL_EMPTY_MV 3386U
#define BAT_CALIB_REAL_VOLTAGE 1260U
#define BAT_CALIB_ADC 1275U

#define LEFT_TIM TIM8
#define RIGHT_TIM TIM1
