#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "board/can.h"
#include "board/health.h"
#include "board/body/boards/board_declarations.h"
#include "board/drivers/can_common_declarations.h"
#include "opendbc/safety/declarations.h"
#include "board/body/bldc/bldc.h"

#define BODY_CAN_ADDR_MOTOR_SPEED        0x201U
#define BODY_CAN_MOTOR_SPEED_PERIOD_US   10000U
#define BODY_CAN_CMD_TIMEOUT_US          100000U
#define BODY_BUS_NUMBER                  0U

static uint32_t last_can_cmd_timestamp_us = 0U;

void body_can_send_motor_speeds(uint8_t bus, float left_speed_rpm, float right_speed_rpm) {
  static uint16_t counter = 0U;
  CANPacket_t pkt;
  pkt.bus = bus;
  pkt.addr = BODY_CAN_ADDR_MOTOR_SPEED;
  pkt.returned = 0;
  pkt.rejected = 0;
  pkt.extended = 0;
  pkt.fd = 0;
  pkt.data_len_code = 8;
  int16_t left_speed_deci = left_speed_rpm * 10;
  int16_t right_speed_deci = right_speed_rpm * 10;
  pkt.data[0] = (uint8_t)((left_speed_deci >> 8) & 0xFFU);
  pkt.data[1] = (uint8_t)(left_speed_deci & 0xFFU);
  pkt.data[2] = (uint8_t)((right_speed_deci >> 8) & 0xFFU);
  pkt.data[3] = (uint8_t)(right_speed_deci & 0xFFU);
  pkt.data[4] = 0U;
  pkt.data[5] = 0U;
  pkt.data[6] = counter & 0xFFU;
  can_set_checksum(&pkt);
  can_send(&pkt, bus, true);
  counter++;
}

void body_can_process_target(int16_t left_target_deci_rpm, int16_t right_target_deci_rpm) {
  rpml = (int)(((float)left_target_deci_rpm) * 0.1f);
  rpmr = (int)(((float)right_target_deci_rpm) * 0.1f);
  last_can_cmd_timestamp_us = microsecond_timer_get();
}

void body_can_init(void) {
  last_can_cmd_timestamp_us = 0U;
  can_silent = false;
  can_loopback = false;
  (void)set_safety_hooks(SAFETY_BODY, 0U);
  set_gpio_output(CAN_TRANSCEIVER_EN_PORT, CAN_TRANSCEIVER_EN_PIN, 0); // Enable CAN transceiver
  can_init_all();
}

void body_can_periodic(uint32_t now) {
  if ((last_can_cmd_timestamp_us != 0U) &&
      ((now - last_can_cmd_timestamp_us) >= BODY_CAN_CMD_TIMEOUT_US)) {
    rpml = 0;
    rpmr = 0;
    last_can_cmd_timestamp_us = 0U;
  }

  static uint32_t last_motor_speed_tx_us = 0;
  if ((now - last_motor_speed_tx_us) >= BODY_CAN_MOTOR_SPEED_PERIOD_US) {
    float left_speed_rpm = motor_encoder_get_speed_rpm(BODY_MOTOR_LEFT);
    float right_speed_rpm = motor_encoder_get_speed_rpm(BODY_MOTOR_RIGHT);
    body_can_send_motor_speeds(BODY_BUS_NUMBER, left_speed_rpm, right_speed_rpm);
    last_motor_speed_tx_us = now;
  }
}
