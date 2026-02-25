#pragma once

#include <stdbool.h>
#include <stdint.h>

#define BODY_CAN_ADDR_MOTOR_SPEED      0x201U
#define BODY_CAN_MOTOR_SPEED_PERIOD_US 10000U
#define BODY_BUS_NUMBER                 0U

void body_can_send_motor_speeds(uint8_t bus, float left_speed_rpm, float right_speed_rpm);
void body_can_process_target(int16_t left_target_deci_rpm, int16_t right_target_deci_rpm);
void body_can_init(void);
void body_can_periodic(uint32_t now);
