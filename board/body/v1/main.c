#include <stdint.h>
#include <stdbool.h>

void print(const char *a);

#include "board/libc.h"
#include "stm32f4xx_hal.h"

#include "config.h"
#include "board/body/bldc/v1/bldc_defs.h"
#include "defines.h"
#include "setup.h"
#include "util.h"
#include "comms.h"
#include "drivers/clock.h"
#include "early_init.h"
#include "boards.h"
#include "../body_common.h"

#include "bldc/BLDC_controller.h"

extern TIM_HandleTypeDef htim_left;
extern TIM_HandleTypeDef htim_right;
extern ADC_HandleTypeDef hadc;
extern volatile adc_buf_t adc_buffer;

// Matlab defines - from auto-code generation
extern P rtP_Left;
extern P rtP_Right;
extern ExtY rtY_Left;
extern ExtY rtY_Right;
extern ExtU rtU_Left;
extern ExtU rtU_Right;

extern int16_t speedAvg;
extern int16_t speedAvgAbs;

extern volatile int pwml;
extern volatile int pwmr;
extern uint8_t enable_motors;
extern int16_t batVoltage;
extern board_t board;

volatile uint32_t torque_cmd_timeout = 0U;
volatile uint32_t ignition_off_counter = 0U;

uint16_t cnt_press = 0U;

int16_t batVoltageCalib;
int16_t board_temp_deg_c;
volatile int16_t cmdL = 0;
volatile int16_t cmdR = 0;

uint8_t hw_type = HW_TYPE_BODY;
uint8_t ignition = 0U;
uint8_t charger_connected = 0U;
uint8_t pkt_idx = 0U;

static uint32_t tick_prev = 0U;
static uint32_t main_loop_1Hz = 0U;
static uint32_t main_loop_1Hz_runtime = 0U;
static uint32_t main_loop_10Hz = 0U;
static uint32_t main_loop_10Hz_runtime = 0U;
static uint32_t main_loop_100Hz = 0U;
static uint32_t main_loop_100Hz_runtime = 0U;

void __initialize_hardware_early(void) {
  early_initialization();
}

static void handle_motor_commands(void) {
  if (ABS(cmdL) < 10) {
    rtP_Left.n_cruiseMotTgt = 0;
    rtP_Left.b_cruiseCtrlEna = 1;
  } else {
    rtP_Left.b_cruiseCtrlEna = 0;
    pwml = CLAMP((int)cmdL, -TORQUE_BASE_MAX, TORQUE_BASE_MAX);
  }

  if (ABS(cmdR) < 10) {
    rtP_Right.n_cruiseMotTgt = 0;
    rtP_Right.b_cruiseCtrlEna = 1;
  } else {
    rtP_Right.b_cruiseCtrlEna = 0;
    pwmr = -CLAMP((int)cmdR, -TORQUE_BASE_MAX, TORQUE_BASE_MAX);
  }
}

static void send_motor_packets(void) {
  uint8_t dat[8];

  // MOTORS_DATA: speed_L(2), speed_R(2), reserved(2), counter(1), checksum(1)
  int16_t speedL = rtY_Left.n_mot;
  int16_t speedR = -(rtY_Right.n_mot);
  body_pack_motor_speed_data(dat, speedL, speedR, pkt_idx);
  dat[7] = crc_checksum(dat, 7, crc_poly);
  can_send_msg(BODY_CAN_ADDR_MOTOR_SPEED + board.can_addr_offset,
               ((dat[7] << 24U) | (dat[6] << 16U) | (dat[5] << 8U) | dat[4]),
               ((dat[3] << 24U) | (dat[2] << 16U) | (dat[1] << 8U) | dat[0]),
               8U);
  pkt_idx = (pkt_idx + 1U) & 0xFU;

  // MOTORS_CURRENT: left_pha_ab(2), left_pha_bc(2), right_pha_ab(2), right_pha_bc(2)
  body_pack_motor_current_data(dat, rtU_Left.i_phaAB, rtU_Left.i_phaBC, rtU_Right.i_phaAB, rtU_Right.i_phaBC);
  can_send_msg(BODY_CAN_ADDR_MOTOR_CURRENT + board.can_addr_offset,
               ((dat[7] << 24U) | (dat[6] << 16U) | (dat[5] << 8U) | dat[4]),
               ((dat[3] << 24U) | (dat[2] << 16U) | (dat[1] << 8U) | dat[0]),
               8U);
}

int main(void) {
  HAL_Init();
  HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
  HAL_NVIC_SetPriority(MemoryManagement_IRQn, 0, 0);
  HAL_NVIC_SetPriority(BusFault_IRQn, 0, 0);
  HAL_NVIC_SetPriority(UsageFault_IRQn, 0, 0);
  HAL_NVIC_SetPriority(SVCall_IRQn, 0, 0);
  HAL_NVIC_SetPriority(DebugMonitor_IRQn, 0, 0);
  HAL_NVIC_SetPriority(PendSV_IRQn, 0, 0);
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);

  SystemClock_Config();
  MX_GPIO_Clocks_Init();

  __HAL_RCC_DMA2_CLK_DISABLE();

  board_detect();
  MX_GPIO_Common_Init();
  MX_TIM_Init();
  MX_ADC_Init();
  BLDC_Init();

  HAL_ADC_Start(&hadc);

  out_enable(POWERSWITCH, true);
  out_enable(IGNITION, ignition);
  out_enable(TRANSCEIVER, true);
  while (HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN)) { HAL_Delay(10); }

  out_enable(LED_RED, false);
  out_enable(LED_GREEN, false);
  out_enable(LED_BLUE, false);

  llcan_set_speed(board.CAN, 5000, false, false);
  llcan_init(board.CAN);

  poweronMelody();

  int32_t board_temp_adcFixdt = ((int32_t)adc_buffer.temp) << 16;
  int16_t board_temp_adcFilt = adc_buffer.temp;

  while (1) {
    if ((HAL_GetTick() - tick_prev) >= 1U) {
      if ((HAL_GetTick() - (main_loop_100Hz - main_loop_100Hz_runtime)) >= 10U) {
        main_loop_100Hz_runtime = HAL_GetTick();

        calcAvgSpeed();

        if (ignition == 0U) {
          cmdL = 0;
          cmdR = 0;
          enable_motors = 0U;
        }

        if ((enable_motors == 0U) || (torque_cmd_timeout > 10U)) {
          cmdL = 0;
          cmdR = 0;
        }

        if ((ignition == 1U) && (enable_motors == 0U) &&
            (rtY_Left.z_errCode == 0U) && (rtY_Right.z_errCode == 0U) &&
            (ABS(cmdL) < 50) && (ABS(cmdR) < 50)) {
          beepShort(6);
          beepShort(4);
          HAL_Delay(100);
          cmdL = 0;
          cmdR = 0;
          enable_motors = 1U;
        }

        handle_motor_commands();

        if (ignition_off_counter <= IGNITION_OFF_DELAY) {
          send_motor_packets();
        }

        torque_cmd_timeout = (torque_cmd_timeout < MAX_uint32_T) ? (torque_cmd_timeout + 1U) : 0U;
        main_loop_100Hz_runtime = HAL_GetTick() - main_loop_100Hz_runtime;
        main_loop_100Hz = HAL_GetTick();
      }

      if ((HAL_GetTick() - (main_loop_10Hz - main_loop_10Hz_runtime)) >= 100U) {
        main_loop_10Hz_runtime = HAL_GetTick();

        if (ignition_off_counter <= IGNITION_OFF_DELAY) {
          uint8_t dat[3];
          body_pack_var_values_data(dat, ignition != 0U, enable_motors != 0U, 0U, rtY_Left.z_errCode, rtY_Right.z_errCode);
          can_send_msg(BODY_CAN_ADDR_VAR_VALUES + board.can_addr_offset,
                       0x0U,
                       ((dat[2] << 16U) | (dat[1] << 8U) | dat[0]),
                       3U);
        }

        out_enable(LED_GREEN, ignition != 0U);
        main_loop_10Hz_runtime = HAL_GetTick() - main_loop_10Hz_runtime;
        main_loop_10Hz = HAL_GetTick();
      }

      if ((HAL_GetTick() - (main_loop_1Hz - main_loop_1Hz_runtime)) >= 1000U) {
        main_loop_1Hz_runtime = HAL_GetTick();

        filtLowPass32(adc_buffer.temp, TEMP_FILT_COEF, &board_temp_adcFixdt);
        board_temp_adcFilt = (int16_t)(board_temp_adcFixdt >> 16);
        board_temp_deg_c = (TEMP_CAL_HIGH_DEG_C - TEMP_CAL_LOW_DEG_C) *
                           (board_temp_adcFilt - TEMP_CAL_LOW_ADC) /
                           (TEMP_CAL_HIGH_ADC - TEMP_CAL_LOW_ADC) + TEMP_CAL_LOW_DEG_C;

        batVoltageCalib = batVoltage * BAT_CALIB_REAL_VOLTAGE / BAT_CALIB_ADC;
        charger_connected = !HAL_GPIO_ReadPin(CHARGER_PORT, CHARGER_PIN);

        int battery_percent = 100 - (int)((((420 * BAT_CELLS) - batVoltageCalib) / BAT_CELLS) / VOLTS_PER_PERCENT / 100);
        battery_percent = CLAMP(battery_percent, 0, 100);

        uint8_t dat[4];
        body_pack_body_data(dat, (uint8_t)board_temp_deg_c, (uint16_t)batVoltageCalib, (uint8_t)battery_percent, charger_connected != 0U);
        can_send_msg(BODY_CAN_ADDR_BODY_DATA + board.can_addr_offset,
                     0x0U,
                     ((dat[3] << 24U) | (dat[2] << 16U) | (dat[1] << 8U) | dat[0]),
                     4U);

        out_enable(LED_BLUE, false);
        out_enable(LED_GREEN, true);

        if (ignition != 0U) {
          ignition_off_counter = 0U;
        } else {
          ignition_off_counter = (ignition_off_counter < MAX_uint32_T) ? (ignition_off_counter + 1U) : 0U;
        }

        if ((TEMP_POWEROFF_ENABLE && (board_temp_deg_c >= TEMP_POWEROFF) && (speedAvgAbs < 20)) ||
            ((batVoltage < BAT_DEAD) && (speedAvgAbs < 20))) {
          poweroff();
        } else if ((rtY_Left.z_errCode != 0U) || (rtY_Right.z_errCode != 0U)) {
          enable_motors = 0U;
          beepCount(1, 24, 1);
        } else if (TEMP_WARNING_ENABLE && (board_temp_deg_c >= TEMP_WARNING)) {
          beepCount(5, 24, 1);
        } else if (batVoltage < BAT_LVL1) {
          beepCount(0, 10, 6);
          out_enable(LED_RED, true);
        } else if (batVoltage < BAT_LVL2) {
          beepCount(0, 10, 30);
          out_enable(LED_RED, false);
        } else {
          beepCount(0, 0, 0);
          out_enable(LED_RED, false);
        }

        main_loop_1Hz_runtime = HAL_GetTick() - main_loop_1Hz_runtime;
        main_loop_1Hz = HAL_GetTick();
      }

      if (HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN)) {
        cnt_press += 1U;
        if (cnt_press == (2U * 1008U)) {
          poweroff();
        }
      } else if (cnt_press >= 10U) {
        ignition = !ignition;
        out_enable(IGNITION, ignition != 0U);
        beepShort(5);
        cnt_press = 0U;
      } else {
        cnt_press = 0U;
      }

      process_can();
      tick_prev = HAL_GetTick();
    }
  }
}
