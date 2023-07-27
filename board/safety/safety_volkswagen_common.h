#ifndef SAFETY_VOLKSWAGEN_COMMON_H
#define SAFETY_VOLKSWAGEN_COMMON_H

const uint16_t FLAG_VOLKSWAGEN_LONG_CONTROL = 1;

uint8_t volkswagen_crc8_lut_8h2f[256]; // Static lookup table for CRC8 poly 0x2F, aka 8H2F/AUTOSAR
bool volkswagen_longitudinal = false;
bool volkswagen_set_button_prev = false;
bool volkswagen_resume_button_prev = false;
bool volkswagen_brake_pedal_switch = false;
bool volkswagen_brake_pressure_detected = false;

// Shared between MQB and MQBevo
#define VW_MSG_LH_EPS_03        0x09F   // RX from EPS, for driver steering torque
#define VW_MSG_HCA_01           0x126   // TX by OP, Heading Control Assist steering torque
#define VW_MSG_GRA_ACC_01       0x12B   // TX by OP, ACC control buttons for cancel/resume
#define VW_MSG_LDW_02           0x397   // TX by OP, Lane line recognition and text alerts
#define VW_MSG_MOTOR_14         0x3BE   // RX from ECU, for brake switch status

// MQB
#define VW_MSG_ESP_19           0x0B2   // RX from ABS, for wheel speeds
#define VW_MSG_ESP_05           0x106   // RX from ABS, for brake switch state
#define VW_MSG_TSK_06           0x120   // RX from ECU, for ACC status from drivetrain coordinator
#define VW_MSG_MOTOR_20         0x121   // RX from ECU, for driver throttle input
#define VW_MSG_ACC_06           0x122   // TX by OP, ACC control instructions to the drivetrain coordinator
#define VW_MSG_ACC_07           0x12E   // TX by OP, ACC control instructions to the drivetrain coordinator
#define VW_MSG_ACC_02           0x30C   // TX by OP, ACC HUD data to the instrument cluster

// MQBevo
#define VW_MSG_ESP_NEW_1        0xFC    // RX from ABS, for wheel speeds
#define VW_MSG_MOTOR_NEW_1      0x10B   // RX from ECU, for ACC status from drivetrain coordinator
#define VW_MSG_ESP_NEW_3        0x139   // RX from ABS, for brake pressure and brake pressed state
#define VW_MSG_HCA_NEW          0x303   // TX by OP, steering torque control message

// PQ25/PQ35/PQ46/NMS
#define VW_MSG_LENKHILFE_3      0x0D0   // RX from EPS, for steering angle and driver steering torque
#define VW_MSG_HCA_1            0x0D2   // TX by OP, Heading Control Assist steering torque
#define VW_MSG_BREMSE_1         0x1A0   // RX from ABS, for ego speed
#define VW_MSG_MOTOR_2          0x288   // RX from ECU, for CC state and brake switch state
#define VW_MSG_ACC_SYSTEM       0x368   // TX by OP, longitudinal acceleration controls
#define VW_MSG_MOTOR_3          0x380   // RX from ECU, for driver throttle input
#define VW_MSG_GRA_NEU          0x38A   // TX by OP, ACC control buttons for cancel/resume
#define VW_MSG_MOTOR_5          0x480   // RX from ECU, for ACC main switch state
#define VW_MSG_ACC_GRA_ANZEIGE  0x56A   // TX by OP, ACC HUD
#define VW_MSG_LDW_1            0x5BE   // TX by OP, Lane line recognition and text alerts


static void volkswagen_common_init(uint16_t param) {
#ifdef ALLOW_DEBUG
  volkswagen_longitudinal = GET_FLAG(param, FLAG_VOLKSWAGEN_LONG_CONTROL);
#else
  UNUSED(param);
#endif

  gen_crc_lookup_table_8(0x2F, volkswagen_crc8_lut_8h2f);
  volkswagen_set_button_prev = false;
  volkswagen_resume_button_prev = false;
  volkswagen_brake_pedal_switch = false;
  volkswagen_brake_pressure_detected = false;
  return;
}

static uint32_t volkswagen_mqb_get_checksum(CANPacket_t *to_push) {
  return (uint8_t)GET_BYTE(to_push, 0);
}

static uint8_t volkswagen_mqb_get_counter(CANPacket_t *to_push) {
  // MQB message counters are consistently found at LSB 8.
  return (uint8_t)GET_BYTE(to_push, 1) & 0xFU;
}

static uint32_t volkswagen_mqb_compute_crc(CANPacket_t *to_push) {
  int addr = GET_ADDR(to_push);
  int len = GET_LEN(to_push);

  // This is CRC-8H2F/AUTOSAR with a twist. See the OpenDBC implementation
  // of this algorithm for a version with explanatory comments.

  uint8_t crc = 0xFFU;
  for (int i = 1; i < len; i++) {
    crc ^= (uint8_t)GET_BYTE(to_push, i);
    crc = volkswagen_crc8_lut_8h2f[crc];
  }

  uint8_t counter = volkswagen_mqb_get_counter(to_push);
  switch(addr) {
    case VW_MSG_LH_EPS_03:
      crc ^= (uint8_t[]){0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5}[counter];
      break;
    case VW_MSG_ESP_05:
      crc ^= (uint8_t[]){0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07}[counter];
      break;
    case VW_MSG_TSK_06:
      crc ^= (uint8_t[]){0xC4,0xE2,0x4F,0xE4,0xF8,0x2F,0x56,0x81,0x9F,0xE5,0x83,0x44,0x05,0x3F,0x97,0xDF}[counter];
      break;
    case VW_MSG_MOTOR_20:
      crc ^= (uint8_t[]){0xE9,0x65,0xAE,0x6B,0x7B,0x35,0xE5,0x5F,0x4E,0xC7,0x86,0xA2,0xBB,0xDD,0xEB,0xB4}[counter];
      break;
    default: // Undefined CAN message, CRC check expected to fail
      break;
  }
  crc = volkswagen_crc8_lut_8h2f[crc];

  return (uint8_t)(crc ^ 0xFFU);
}

#endif
