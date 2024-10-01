#include "safety_volkswagen_common.h"

// lateral limits for curvature
//const SteeringLimits VOLKSWAGEN_MEB_STEERING_LIMITS = {
//  // SG_ Curvature : 24|15@1+ (0.00625,0) [0|200] "Unit_1/mm" XXX
//  // we do not enforce curvature error at the moment
//  .max_steer = 31200,         // maximum curvature of 195 1/mm
//  .angle_deg_to_can = 160,    // 1 / 0.00625 rad to can
//  .angle_rate_up_lookup = {
//    {5., 12., 25.},
//    {4, 2, 1}
//  },
//  .angle_rate_down_lookup = {
//    {5., 12., 25.},
//    {5, 2.5, 1.5}
//  },
//  .inactive_angle_is_zero = true,
//};

// lateral limits for angle
const SteeringLimits VOLKSWAGEN_MEB_STEERING_LIMITS = {
  // SG_ Steering_Angle : 24|15@1+ (0.0174,0) [0|360] "Unit_DegreOfArc" XXX
  .max_steer = 2068966, // 360 deg
  .angle_deg_to_can = 5747, // (1 / 0.0174) * 100 deg to can (minimize rounding error)
  .angle_rate_up_lookup = {
    {0., 5., 15.},
    {1200., 400., 40.}
  },
  .angle_rate_down_lookup = {
    {0., 5., 15.},
    {1200., 800., 80.}
  },
  .inactive_angle_is_zero = true,
};

// longitudinal limits
// acceleration in m/s2 * 1000 to avoid floating point math
const LongitudinalLimits VOLKSWAGEN_MEB_LONG_LIMITS = {
  .max_accel = 2000,
  .min_accel = -3500,
  .inactive_accel = 3010,  // VW sends one increment above the max range when inactive
};

#define MSG_MEB_ESP_01           0xFC    // RX, for wheel speeds
#define MSG_MEB_ESP_03           0x14C   // RX, for accel pedal
#define MSG_MEB_ESP_05           0x139   // RX, for ESP hold management
#define MSG_MEB_ABS_01           0x20A   // RX, for yaw rate
#define MSG_HCA_03               0x303   // TX by OP, Heading Control Assist steering torque
#define MSG_MEB_EPS_01           0x13D   // RX, for steering angle
#define MSG_MEB_ACC_01           0x300   // RX from ECU, for ACC status
#define MSG_MEB_ACC_02           0x14D   // RX from ECU, for ACC status
#define MSG_GRA_ACC_01           0x12B   // TX by OP, ACC control buttons for cancel/resume
#define MSG_MOTOR_14             0x3BE   // RX from ECU, for brake switch status
#define MSG_LDW_02               0x397   // TX by OP, Lane line recognition and text alerts
#define MSG_MEB_MOTOR_01         0x10B   // RX for TSK state
#define MSG_MEB_TRAVEL_ASSIST_01 0x26B   // TX for Travel Assist status


// Transmit of GRA_ACC_01 is allowed on bus 0 and 2 to keep compatibility with gateway and camera integration
const CanMsg VOLKSWAGEN_MEB_STOCK_TX_MSGS[] = {{MSG_HCA_03, 0, 24}, {MSG_GRA_ACC_01, 0, 8},
                                               {MSG_GRA_ACC_01, 2, 8}, {MSG_LDW_02, 0, 8}, {MSG_LH_EPS_03, 2, 8}};
const CanMsg VOLKSWAGEN_MEB_LONG_TX_MSGS[] = {{MSG_MEB_ACC_01, 0, 48}, {MSG_MEB_ACC_02, 0, 32}, {MSG_HCA_03, 0, 24},
                                              {MSG_LDW_02, 0, 8}, {MSG_LH_EPS_03, 2, 8}, {MSG_MEB_TRAVEL_ASSIST_01, 0, 8}};

RxCheck volkswagen_meb_rx_checks[] = {
  {.msg = {{MSG_LH_EPS_03, 0, 8, .check_checksum = true, .max_counter = 15U, .frequency = 100U}, { 0 }, { 0 }}},
  {.msg = {{MSG_MOTOR_14, 0, 8, .check_checksum = true, .max_counter = 15U, .frequency = 10U}, { 0 }, { 0 }}},
  {.msg = {{MSG_MEB_MOTOR_01, 0, 32, .check_checksum = true, .max_counter = 15U, .frequency = 50U}, { 0 }, { 0 }}},
  {.msg = {{MSG_GRA_ACC_01, 0, 8, .check_checksum = true, .max_counter = 15U, .frequency = 33U}, { 0 }, { 0 }}},
  {.msg = {{MSG_MEB_EPS_01, 0, 32, .check_checksum = true, .max_counter = 15U, .frequency = 100U}, { 0 }, { 0 }}},
  {.msg = {{MSG_MEB_ESP_01, 0, 48, .check_checksum = true, .max_counter = 15U, .frequency = 100U}, { 0 }, { 0 }}},
  {.msg = {{MSG_MEB_ESP_03, 0, 32, .check_checksum = true, .max_counter = 15U, .frequency = 10U}, { 0 }, { 0 }}},
  {.msg = {{MSG_MEB_ESP_05, 0, 32, .check_checksum = true, .max_counter = 15U, .frequency = 50U}, { 0 }, { 0 }}},
  {.msg = {{MSG_MEB_ABS_01, 0, 64, .check_checksum = true, .max_counter = 15U, .frequency = 50U}, { 0 }, { 0 }}},
};

uint8_t volkswagen_crc8_lut_8h2f[256]; // Static lookup table for CRC8 poly 0x2F, aka 8H2F/AUTOSAR
int volkswagen_steer_power_prev = 0;
bool volkswagen_esp_hold_confirmation = false;
const int volkswagen_accel_overwrite = 0;

bool vw_meb_get_longitudinal_allowed_override(void) {
  return controls_allowed && controls_allowed_long && gas_pressed_prev;
}

// Safety checks for longitudinal actuation
bool vw_meb_longitudinal_accel_checks(int desired_accel, const LongitudinalLimits limits, const int override_accel) {
  bool accel_valid = get_longitudinal_allowed() && !max_limit_check(desired_accel, limits.max_accel, limits.min_accel);
  bool accel_valid_override = vw_meb_get_longitudinal_allowed_override() && desired_accel == override_accel;
  bool accel_inactive = desired_accel == limits.inactive_accel;
  return !(accel_valid || accel_inactive || accel_valid_override);
}

static uint32_t volkswagen_meb_get_checksum(const CANPacket_t *to_push) {
  return (uint8_t)GET_BYTE(to_push, 0);
}

static uint8_t volkswagen_meb_get_counter(const CANPacket_t *to_push) {
  // MQB message counters are consistently found at LSB 8.
  return (uint8_t)GET_BYTE(to_push, 1) & 0xFU;
}

static uint32_t volkswagen_meb_compute_crc(const CANPacket_t *to_push) {
  int addr = GET_ADDR(to_push);
  int len = GET_LEN(to_push);

  // This is CRC-8H2F/AUTOSAR with a twist. See the OpenDBC implementation
  // of this algorithm for a version with explanatory comments.

  uint8_t crc = 0xFFU;
  for (int i = 1; i < len; i++) {
    crc ^= (uint8_t)GET_BYTE(to_push, i);
    crc = volkswagen_crc8_lut_8h2f[crc];
  }
  
  uint8_t counter = volkswagen_meb_get_counter(to_push);
  if (addr == MSG_LH_EPS_03) {
    crc ^= (uint8_t[]){0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5,0xF5}[counter];
  } else if (addr == MSG_GRA_ACC_01) {
    crc ^= (uint8_t[]){0x6A,0x38,0xB4,0x27,0x22,0xEF,0xE1,0xBB,0xF8,0x80,0x84,0x49,0xC7,0x9E,0x1E,0x2B}[counter];
  } else if (addr == MSG_MEB_EPS_01) {
    crc ^= (uint8_t[]){0x20,0xCA,0x68,0xD5,0x1B,0x31,0xE2,0xDA,0x08,0x0A,0xD4,0xDE,0x9C,0xE4,0x35,0x5B}[counter];
  } else if (addr == MSG_MEB_ESP_01) {
    crc ^= (uint8_t[]){0x77,0x5C,0xA0,0x89,0x4B,0x7C,0xBB,0xD6,0x1F,0x6C,0x4F,0xF6,0x20,0x2B,0x43,0xDD}[counter];
  } else if (addr == MSG_MEB_ESP_03) {
    crc ^= (uint8_t[]){0x16,0x35,0x59,0x15,0x9A,0x2A,0x97,0xB8,0x0E,0x4E,0x30,0xCC,0xB3,0x07,0x01,0xAD}[counter];
  } else if (addr == MSG_MEB_ESP_05) {
    crc ^= (uint8_t[]){0xED,0x03,0x1C,0x13,0xC6,0x23,0x78,0x7A,0x8B,0x40,0x14,0x51,0xBF,0x68,0x32,0xBA}[counter];
  } else if (addr == MSG_MEB_MOTOR_01) {
    crc ^= (uint8_t[]){0x77,0x5C,0xA0,0x89,0x4B,0x7C,0xBB,0xD6,0x1F,0x6C,0x4F,0xF6,0x20,0x2B,0x43,0xDD}[counter];
  } else if (addr == MSG_MOTOR_14) {
    crc ^= (uint8_t[]){0x1F,0x28,0xC6,0x85,0xE6,0xF8,0xB0,0x19,0x5B,0x64,0x35,0x21,0xE4,0xF7,0x9C,0x24}[counter];
  } else if (addr == MSG_MEB_ABS_01) {
    crc ^= (uint8_t[]){0x9D,0xE8,0x36,0xA1,0xCA,0x3B,0x1D,0x33,0xE0,0xD5,0xBB,0x5F,0xAE,0x3C,0x31,0x9F}[counter];
  } else {
    // Undefined CAN message, CRC check expected to fail
  }
  crc = volkswagen_crc8_lut_8h2f[crc];

  return (uint8_t)(crc ^ 0xFFU);
}

static safety_config volkswagen_meb_init(uint16_t param) {
  UNUSED(param);

  volkswagen_set_button_prev = false;
  volkswagen_resume_button_prev = false;
  volkswagen_steer_power_prev = 0;

#ifdef ALLOW_DEBUG
  volkswagen_longitudinal = GET_FLAG(param, FLAG_VOLKSWAGEN_LONG_CONTROL);
#endif
  gen_crc_lookup_table_8(0x2F, volkswagen_crc8_lut_8h2f);
  return volkswagen_longitudinal ? BUILD_SAFETY_CFG(volkswagen_meb_rx_checks, VOLKSWAGEN_MEB_LONG_TX_MSGS) : \
                                   BUILD_SAFETY_CFG(volkswagen_meb_rx_checks, VOLKSWAGEN_MEB_STOCK_TX_MSGS);
}

static void volkswagen_meb_rx_hook(const CANPacket_t *to_push) {
  if (GET_BUS(to_push) == 0U) {
    int addr = GET_ADDR(to_push);

    // Update in-motion state by sampling wheel speeds
    if (addr == MSG_MEB_ESP_01) {
      uint32_t fr = GET_BYTE(to_push, 10U) | GET_BYTE(to_push, 11U) << 8;
      uint32_t rr = GET_BYTE(to_push, 14U) | GET_BYTE(to_push, 15U) << 8;
      uint32_t rl = GET_BYTE(to_push, 12U) | GET_BYTE(to_push, 13U) << 8;
      uint32_t fl = GET_BYTE(to_push, 8U) | GET_BYTE(to_push, 9U) << 8;

      vehicle_moving = (fr > 0U) || (rr > 0U) || (rl > 0U) || (fl > 0U);

      UPDATE_VEHICLE_SPEED(((fr + rr + rl + fl) / 4 ) * 0.0075 / 3.6);
    }

    // get ESP hold confirmation
    if (addr == MSG_MEB_ESP_05) {
      volkswagen_esp_hold_confirmation = GET_BIT(to_push, 35U);
    }

    // Update steering input angle samples
    if (addr == MSG_MEB_EPS_01) {
      // use factor 100 to match steering request for safety checks
      float angle_meas_new = (((GET_BYTE(to_push, 9U) & 0xF0U) >> 4) | (GET_BYTE(to_push, 10U) << 4) | ((GET_BYTE(to_push, 11U) & 0x1FU) << 12)) * 0.906;
      int sign = GET_BIT(to_push, 55U);
      if (sign == 1) {
        angle_meas_new *= -1;
      }
      update_sample(&angle_meas, ROUND(angle_meas_new * VOLKSWAGEN_MEB_STEERING_LIMITS.angle_deg_to_can));
    }

    // Update vehicle yaw rate for curvature checks
    //if (addr == MSG_MEB_ABS_01) {
    //  float volkswagen_yaw_rate = (((GET_BYTE(to_push, 25U) | (GET_BYTE(to_push, 26U) << 8 )) * 0.007) - 229.36) * 0.0174533;
    //  float current_curvature = volkswagen_yaw_rate / MAX(vehicle_speed.values[0] / VEHICLE_SPEED_FACTOR, 0.1);
    //  // convert current curvature into units on CAN for comparison with desired curvature
    //  update_sample(&angle_meas, ROUND(current_curvature * VOLKSWAGEN_MEB_STEERING_LIMITS.angle_deg_to_can));
    //}

    // Update cruise state
    if (addr == MSG_MEB_MOTOR_01) {
      // When using stock ACC, enter controls on rising edge of stock ACC engage, exit on disengage
      // Always exit controls on main switch off
      // Signal: TSK_06.TSK_Status
      int acc_status = ((GET_BYTE(to_push, 11U) >> 0) & 0x07U);
      bool cruise_engaged = (acc_status == 3) || (acc_status == 4) || (acc_status == 5);
      acc_main_on = cruise_engaged || (acc_status == 2);

      if (!volkswagen_longitudinal) {
        pcm_cruise_check(cruise_engaged);
      }

      if (!acc_main_on) {
        controls_allowed = false;
      }
    }

    // update cruise buttons
    if (addr == MSG_GRA_ACC_01) {
      // If using openpilot longitudinal, enter controls on falling edge of Set or Resume with main switch on
      // Signal: GRA_ACC_01.GRA_Tip_Setzen
      // Signal: GRA_ACC_01.GRA_Tip_Wiederaufnahme
      if (volkswagen_longitudinal) {
        bool set_button = GET_BIT(to_push, 16U);
        bool resume_button = GET_BIT(to_push, 19U);
        if ((volkswagen_set_button_prev && !set_button) || (volkswagen_resume_button_prev && !resume_button)) {
          controls_allowed = acc_main_on;
        }
        volkswagen_set_button_prev = set_button;
        volkswagen_resume_button_prev = resume_button;
      }
      // Always exit controls on rising edge of Cancel
      // Signal: GRA_ACC_01.GRA_Abbrechen
      if (GET_BIT(to_push, 13U)) {
        controls_allowed = false;
      }
    }

    // update brake pedal
    if (addr == MSG_MOTOR_14) {
      brake_pressed = GET_BIT(to_push, 28U);
    }

    // update accel pedal
    if (addr == MSG_MEB_ESP_03) {
      int accel_pedal_value = GET_BYTE(to_push, 21U) - 37;
      gas_pressed = accel_pedal_value != 0;
    }

    generic_rx_checks((addr == MSG_HCA_03));
  }
}

static bool volkswagen_meb_tx_hook(const CANPacket_t *to_send) {
  int addr = GET_ADDR(to_send);
  bool tx = true;

  // Safety check for HCA_03 Heading Control Assist curvature
  if (addr == MSG_HCA_03) {
    //int desired_curvature_raw = (GET_BYTE(to_send, 3U) | (GET_BYTE(to_send, 4U) & 0x7FU << 8));
    int desired_angle_raw = (GET_BYTE(to_send, 3) | (GET_BYTE(to_send, 4) & 0x7FU << 8));

    bool sign = GET_BIT(to_send, 39U);
    if (sign) {
      desired_angle_raw *= -1;
    }

    bool steer_req = GET_BIT(to_send, 14U);
    int steer_power = (GET_BYTE(to_send, 2U) >> 0) & 0x7FU;

    if (steer_angle_cmd_checks(desired_angle_raw, steer_req, VOLKSWAGEN_MEB_STEERING_LIMITS)) {
      tx = false;

      // steer power is still allowed to decrease to zero monotonously
      // while controls are not allowed anymore
      if (steer_req && steer_power != 0) {        
        if (steer_power < volkswagen_steer_power_prev) {
          tx = true;
        }
      }
    }

    if (!steer_req && steer_power != 0) {
      tx = false; // steer power is not 0 when disabled
    }

    volkswagen_steer_power_prev = steer_power;
  }

  // Safety check for MSG_MEB_ACC_02 acceleration requests
  // To avoid floating point math, scale upward and compare to pre-scaled safety m/s2 boundaries
  if (addr == MSG_MEB_ACC_02) {
    // WARNING: IF WE TAKE THE SIGNAL FROM THE CAR WHILE ACC ACTIVE AND BELOW ABOUT 3km/h, THE CAR ERRORS AND PUTS ITSELF IN PARKING MODE WITH EPB!
    int desired_accel = ((((GET_BYTE(to_send, 4) & 0x7U) << 8) | GET_BYTE(to_send, 3)) * 5U) - 7220U;

    if (vw_meb_longitudinal_accel_checks(desired_accel, VOLKSWAGEN_MEB_LONG_LIMITS, volkswagen_accel_override)) {
      tx = false;
    }
  }

  // FORCE CANCEL: ensuring that only the cancel button press is sent when controls are off.
  // This avoids unintended engagements while still allowing resume spam
  if ((addr == MSG_GRA_ACC_01) && !controls_allowed) {
    // disallow resume and set: bits 16 and 19
    if ((GET_BYTE(to_send, 2) & 0x9U) != 0U) {
      tx = false;
    }
  }

  return tx;
}

static int volkswagen_meb_fwd_hook(int bus_num, int addr) {
  int bus_fwd = -1;

  switch (bus_num) {
    case 0:
      bus_fwd = 2;
      break;
    case 2:
      if ((addr == MSG_HCA_03) || (addr == MSG_LDW_02)) {
        // openpilot takes over LKAS steering control and related HUD messages from the camera
        bus_fwd = -1;
      } else if (volkswagen_longitudinal && ((addr == MSG_MEB_ACC_01) || (addr == MSG_MEB_ACC_02) || (addr == MSG_MEB_TRAVEL_ASSIST_01))) {
        // openpilot takes over acceleration/braking control and related HUD messages from the stock ACC radar
        bus_fwd = -1;
      } else {
        // Forward all remaining traffic from Extended CAN devices to J533 gateway
        bus_fwd = 0;
      }
      break;
    default:
      // No other buses should be in use; fallback to do-not-forward
      bus_fwd = -1;
      break;
  }

  return bus_fwd;
}

const safety_hooks volkswagen_meb_hooks = {
  .init = volkswagen_meb_init,
  .rx = volkswagen_meb_rx_hook,
  .tx = volkswagen_meb_tx_hook,
  .fwd = volkswagen_meb_fwd_hook,
  .get_counter = volkswagen_meb_get_counter,
  .get_checksum = volkswagen_meb_get_checksum,
  .compute_checksum = volkswagen_meb_compute_crc,
};
