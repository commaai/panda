#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "board/can.h"
#include "board/health.h"
#include "board/body/motor_control.h"
#include "board/drivers/can_common_declarations.h"

#define BODY_BUS_NUMBER                 0U

static struct {
  bool pending;
  uint8_t motor;
  int32_t target_deci_rpm;
} body_can_command;

static bool generated_can_traffic = false;

static inline bool body_can_bus_ready(uint8_t bus) {
  uint8_t can_number = CAN_NUM_FROM_BUS_NUM(bus);
  if (bus != 0U || can_number >= PANDA_CAN_CNT) {
    return false;
  }
  const can_health_t *health = &can_health[can_number];
  return (health->bus_off == 0U) && (health->transmit_error_cnt < 128U);
}

static inline bool body_can_send_motor_speeds(uint8_t bus, float left_speed_rpm, float right_speed_rpm) {
  if (!body_can_bus_ready(bus)) {
    generated_can_traffic = true;
    return false;
  }

  CANPacket_t pkt;
  pkt.bus = bus;
  pkt.addr = BODY_CAN_ADDR_MOTOR_SPEED;
  pkt.returned = 0;
  pkt.rejected = 0;
  pkt.extended = 0;
  pkt.fd = 0;
  pkt.data_len_code = 4;
  int16_t left_speed_deci = left_speed_rpm * 10;
  int16_t right_speed_deci = right_speed_rpm * 10;
  pkt.data[0] = (uint8_t)(left_speed_deci & 0xFFU);
  pkt.data[1] = (uint8_t)((left_speed_deci >> 8) & 0xFFU);
  pkt.data[2] = (uint8_t)(right_speed_deci & 0xFFU);
  pkt.data[3] = (uint8_t)((right_speed_deci >> 8) & 0xFFU);
  can_set_checksum(&pkt);
  can_send(&pkt, bus, true);
  generated_can_traffic = true;
  return true;
}

void body_can_safety_rx(const CANPacket_t *msg) {
  if ((msg == NULL) || (msg->extended != 0U) || (msg->bus != 0U)) {
    return;
  }

  if ((msg->addr == BODY_CAN_ADDR_TARGET_RPM) && (dlc_to_len[msg->data_len_code] >= 3U)) {
    uint8_t motor = msg->data[0];
    if (body_motor_is_valid(motor)) {
      int16_t target_deci_rpm = (int16_t)((msg->data[2] << 8U) | msg->data[1]);
      body_can_command.motor = motor;
      body_can_command.target_deci_rpm = (int32_t)target_deci_rpm;
      body_can_command.pending = true;
    }
  }
}

void body_can_init(void) {
  body_can_command.pending = false;
  can_silent = false;
  can_loopback = false;
  current_board->enable_can_transceiver(1, true);
  can_init_all();
}

void body_can_periodic(uint32_t now) {
  uint8_t motor;
  int32_t target_deci_rpm;

  if (body_can_command.pending) {
    body_can_command.pending = false;
    motor = body_can_command.motor;
    target_deci_rpm = body_can_command.target_deci_rpm;
    float target_rpm = ((float)target_deci_rpm) * 0.1f;
    motor_speed_controller_set_target_rpm(motor, target_rpm);
  }

  static uint32_t last_motor_speed_tx_us = 0;
  bool first_update = (last_motor_speed_tx_us == 0);
  if (first_update || ((uint32_t)(now - last_motor_speed_tx_us) >= BODY_CAN_MOTOR_SPEED_PERIOD_US)) {
    float left_speed_rpm = motor_encoder_get_speed_rpm(1);
    float right_speed_rpm = motor_encoder_get_speed_rpm(2);
    bool sent = body_can_send_motor_speeds(BODY_BUS_NUMBER, left_speed_rpm, right_speed_rpm);
    if (sent) {
      last_motor_speed_tx_us = now;
    }
  }
}
