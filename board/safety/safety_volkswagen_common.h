#ifndef SAFETY_VOLKSWAGEN_COMMON_H
#define SAFETY_VOLKSWAGEN_COMMON_H

// Common MLB/MQB messages
#define MSG_ACC_02      0x30C   // TX by OP, ACC HUD data to the instrument cluster
#define MSG_ESP_05      0x106   // RX from ABS, for brake switch state
#define MSG_HCA_01      0x126   // TX by OP, ADAS Heading Control Assist steering torque
#define MSG_LDW_02      0x397   // TX by OP, ADAS Lane line recognition and text alerts
#define MSG_LH_EPS_03   0x09F   // RX from EPS, for driver steering torque

// MLB only messages
#define MSG_ESP_03      0x103   // RX from ABS, for wheel speeds
#define MSG_LS_01       0x10B   // TX by OP, ACC control buttons for cancel/resume
#define MSG_MOTOR_03    0x105   // RX from ECU, for driver throttle input and brake switch status
#define MSG_TSK_02      0x10C   // RX from ECU, for ACC status from drivetrain coordinator

// MQB only messages
#define MSG_ACC_06      0x122   // TX by OP, ACC control instructions to the drivetrain coordinator
#define MSG_ACC_07      0x12E   // TX by OP, ACC control instructions to the drivetrain coordinator
#define MSG_ESP_19      0x0B2   // RX from ABS, for wheel speeds
#define MSG_GRA_ACC_01  0x12B   // TX by OP, ACC control buttons for cancel/resume
#define MSG_MOTOR_14    0x3BE   // RX from ECU, for brake switch status
#define MSG_MOTOR_20    0x121   // RX from ECU, for driver throttle input
#define MSG_TSK_06      0x120   // RX from ECU, for ACC status from drivetrain coordinator


const uint16_t FLAG_VOLKSWAGEN_LONG_CONTROL = 1;
uint8_t volkswagen_crc8_lut_8h2f[256]; // Static lookup table for CRC8 poly 0x2F, aka 8H2F/AUTOSAR
bool volkswagen_longitudinal = false;
bool volkswagen_set_button_prev = false;
bool volkswagen_resume_button_prev = false;
bool volkswagen_brake_pedal_switch = false;
bool volkswagen_brake_pressure_detected = false;


static uint32_t volkswagen_mlb_mqb_get_checksum(CANPacket_t *to_push) {
  return (uint8_t)GET_BYTE(to_push, 0);
}

static uint8_t volkswagen_mlb_mqb_get_counter(CANPacket_t *to_push) {
  return (uint8_t)GET_BYTE(to_push, 1) & 0xFU;
}

static uint32_t volkswagen_mlb_mqb_compute_checksum(CANPacket_t *to_push) {
  int addr = GET_ADDR(to_push);
  int len = GET_LEN(to_push);

  // This is CRC-8H2F/AUTOSAR with a twist. See the OpenDBC implementation
  // of this algorithm for a version with explanatory comments.

  uint8_t crc = 0xFFU;
  for (int i = 1; i < len; i++) {
    crc ^= (uint8_t)GET_BYTE(to_push, i);
    crc = volkswagen_crc8_lut_8h2f[crc];
  }

  uint8_t counter = volkswagen_mlb_mqb_get_counter(to_push);
  switch(addr) {
    case MSG_LH_EPS_03:
      crc ^= (uint8_t[]){0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5}[counter];
      break;
    case MSG_ESP_05:
      crc ^= (uint8_t[]){0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07}[counter];
      break;
    case MSG_TSK_06:
      crc ^= (uint8_t[]){0xC4,0xE2,0x4F,0xE4,0xF8,0x2F,0x56,0x81,0x9F,0xE5,0x83,0x44,0x05,0x3F,0x97,0xDF}[counter];
      break;
    case MSG_MOTOR_20:
      crc ^= (uint8_t[]){0xE9,0x65,0xAE,0x6B,0x7B,0x35,0xE5,0x5F,0x4E,0xC7,0x86,0xA2,0xBB,0xDD,0xEB,0xB4}[counter];
      break;
    default: // Undefined CAN message, CRC check expected to fail
      break;
  }

  crc = volkswagen_crc8_lut_8h2f[crc];
  return (uint8_t)(crc ^ 0xFFU);
}

static int volkswagen_mlb_mqb_driver_input_torque(CANPacket_t *to_push) {
  // Signal: LH_EPS_03.EPS_Lenkmoment (absolute torque)
  // Signal: LH_EPS_03.EPS_VZ_Lenkmoment (direction)
  int torque_driver_new = GET_BYTE(to_push, 5) | ((GET_BYTE(to_push, 6) & 0x1FU) << 8);
  bool sign = GET_BIT(to_push, 55U);
  if (sign) {
    torque_driver_new *= -1;
  }
  return torque_driver_new;
}

static bool volkswagen_mlb_mqb_brake_pressure_threshold(CANPacket_t *to_push) {
  // Signal: ESP_05.ESP_Fahrer_bremst (ESP detected driver brake pressure above threshold)
  bool brake_pressure_threshold = GET_BIT(to_push, 26U);
  return brake_pressure_threshold;
}

#endif
