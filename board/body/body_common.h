#pragma once

#include <stdbool.h>
#include <stdint.h>

#define HW_TYPE_BODY 0xB1U

#define BODY_CAN_ADDR_MOTOR_SPEED   0x201U
#define BODY_CAN_ADDR_VAR_VALUES    0x202U
#define BODY_CAN_ADDR_BODY_DATA     0x203U
#define BODY_CAN_ADDR_MOTOR_CURRENT 0x204U
#define BODY_CAN_ADDR_V2_ID         0x222U
#define BODY_CAN_ADDR_TORQUE_CMD    0x250U
#define BODY_CAN_ADDR_SPEED_LIMIT   0x251U

static inline void body_pack_motor_speed_data(uint8_t dat[8], int16_t left_speed, int16_t right_speed, uint8_t counter) {
  uint16_t left_speed_raw = (uint16_t)left_speed;
  uint16_t right_speed_raw = (uint16_t)right_speed;

  dat[0] = (uint8_t)((left_speed_raw >> 8U) & 0xFFU);
  dat[1] = (uint8_t)(left_speed_raw & 0xFFU);
  dat[2] = (uint8_t)((right_speed_raw >> 8U) & 0xFFU);
  dat[3] = (uint8_t)(right_speed_raw & 0xFFU);
  dat[4] = 0U;
  dat[5] = 0U;
  dat[6] = counter;
}

static inline void body_pack_var_values_data(uint8_t dat[3], bool ignition, bool enable_motors, uint8_t fault,
                                             uint8_t left_z_errcode, uint8_t right_z_errcode) {
  dat[0] = (ignition ? 1U : 0U) | ((enable_motors ? 1U : 0U) << 1U) | ((fault & 0x3FU) << 2U);
  dat[1] = left_z_errcode;
  dat[2] = right_z_errcode;
}

static inline void body_pack_body_data(uint8_t dat[4], uint8_t mcu_temp_raw, uint16_t batt_voltage_raw,
                                       uint8_t batt_percentage, bool charger_connected) {
  dat[0] = mcu_temp_raw;
  dat[1] = (uint8_t)((batt_voltage_raw >> 8U) & 0xFFU);
  dat[2] = (uint8_t)(batt_voltage_raw & 0xFFU);
  dat[3] = (charger_connected ? 1U : 0U) | ((batt_percentage & 0x7FU) << 1U);
}

static inline void body_pack_motor_current_data(uint8_t dat[8], int16_t left_pha_ab, int16_t left_pha_bc,
                                                int16_t right_pha_ab, int16_t right_pha_bc) {
  uint16_t left_pha_ab_raw = (uint16_t)left_pha_ab;
  uint16_t left_pha_bc_raw = (uint16_t)left_pha_bc;
  uint16_t right_pha_ab_raw = (uint16_t)right_pha_ab;
  uint16_t right_pha_bc_raw = (uint16_t)right_pha_bc;

  dat[0] = (uint8_t)((left_pha_ab_raw >> 8U) & 0xFFU);
  dat[1] = (uint8_t)(left_pha_ab_raw & 0xFFU);
  dat[2] = (uint8_t)((left_pha_bc_raw >> 8U) & 0xFFU);
  dat[3] = (uint8_t)(left_pha_bc_raw & 0xFFU);
  dat[4] = (uint8_t)((right_pha_ab_raw >> 8U) & 0xFFU);
  dat[5] = (uint8_t)(right_pha_ab_raw & 0xFFU);
  dat[6] = (uint8_t)((right_pha_bc_raw >> 8U) & 0xFFU);
  dat[7] = (uint8_t)(right_pha_bc_raw & 0xFFU);
}

static inline void body_unpack_rpm_targets(const uint8_t dat[4], int16_t *left_target, int16_t *right_target) {
  *left_target = (int16_t)(((uint16_t)dat[0] << 8U) | dat[1]);
  *right_target = (int16_t)(((uint16_t)dat[2] << 8U) | dat[3]);
}

static inline void body_unpack_speed_limits(const uint8_t dat[4], uint16_t *left_limit, uint16_t *right_limit) {
  *left_limit = (uint16_t)(((uint16_t)dat[0] << 8U) | dat[1]);
  *right_limit = (uint16_t)(((uint16_t)dat[2] << 8U) | dat[3]);
}
