const SteeringLimits CHRYSLER_STEERING_LIMITS = {
  .max_steer = 261,
  .max_rt_delta = 112,
  .max_rt_interval = 250000,
  .max_rate_up = 3,
  .max_rate_down = 3,
  .max_torque_error = 80,
  .type = TorqueMotorLimited,
};

const SteeringLimits CHRYSLER_RAM_DT_STEERING_LIMITS = {
  .max_steer = 350,
  .max_rt_delta = 112,
  .max_rt_interval = 250000,
  .max_rate_up = 6,
  .max_rate_down = 6,
  .max_torque_error = 80,
  .type = TorqueMotorLimited,
};

const SteeringLimits CHRYSLER_RAM_HD_STEERING_LIMITS = {
  .max_steer = 361,
  .max_rt_delta = 182,
  .max_rt_interval = 250000,
  .max_rate_up = 14,
  .max_rate_down = 14,
  .max_torque_error = 80,
  .type = TorqueMotorLimited,
};

const LongitudinalLimits CHRYSLER_LONG_LIMITS = {
  // acceleration cmd limits (used for brakes)
  // Signal: ACC_DECEL
  .max_accel = 3685,      // 3685 x 0.004885 - 16 =  2.0 m/s^2
  .min_accel = 2558,      // 2558 x 0.004885 - 16 = -3.5 m/s^2
  .inactive_accel = 4094, // 4094 x 0.004885 - 16 =  4.0 m/s^2

  // gas cmd limits
  // Signal: ENGINE_TORQUE_REQUEST
  .max_gas = 4000,      // 4000 x .25 - 500 =  500.0 Nm
  .min_gas = 0,         //    0 x .25 - 500 = -500.0 Nm
  .inactive_gas = 2000, // 2000 x .25 - 500 =    0.0 Nm
};

enum {
  CHRYSLER_BTN_NONE = 0,
  CHRYSLER_BTN_CANCEL = 1,
  CHRYSLER_BTN_ACCEL = 4,
  CHRYSLER_BTN_DECEL = 8,
  CHRYSLER_BTN_RESUME = 16,
};

bool chrysler_longitudinal = false;

typedef struct {
  const int EPS_2;
  const int ESP_1;
  const int ESP_8;
  const int ECM_5;
  const int DAS_3;
  const int DAS_4;
  const int DAS_5;
  const int DAS_6;
  const int LKAS_COMMAND;
  const int CRUISE_BUTTONS;
} ChryslerAddrs;

// CAN messages for Chrysler/Jeep platforms
const ChryslerAddrs CHRYSLER_ADDRS = {
  .EPS_2            = 0x220,  // EPS driver input torque
  .ESP_1            = 0x140,  // Brake pedal and vehicle speed
  .ESP_8            = 0x11C,  // Brake pedal and vehicle speed
  .ECM_5            = 0x22F,  // Throttle position sensor
  .DAS_3            = 0x1F4,  // ACC state and control from DASM
  .DAS_4            = 0x1F5,  // ACC and FCW dispaly and config from DASM
  .DAS_5            = 0x271,  // ACC and FCW dispaly and config from DASM
  .DAS_6            = 0x2A6,  // LKAS HUD and auto headlight control from DASM
  .LKAS_COMMAND     = 0x292,  // LKAS controls from DASM
  .CRUISE_BUTTONS   = 0x23B,  // Cruise control buttons
};

// CAN messages for the 5th gen RAM DT platform
const ChryslerAddrs CHRYSLER_RAM_DT_ADDRS = {
  .EPS_2            = 0x31,   // EPS driver input torque
  .ESP_1            = 0x83,   // Brake pedal and vehicle speed
  .ESP_8            = 0x79,   // Brake pedal and vehicle speed
  .ECM_5            = 0x9D,   // Throttle position sensor
  .DAS_3            = 0x99,   // ACC state and control from DASM
  .DAS_4            = 0xE8,   // ACC and FCW dispaly and config from DASM
  .DAS_5            = 0xA3,   // ACC and FCW dispaly and config from DASM
  .DAS_6            = 0xFA,   // LKAS HUD and auto headlight control from DASM
  .LKAS_COMMAND     = 0xA6,   // LKAS controls from DASM
  .CRUISE_BUTTONS   = 0xB1,   // Cruise control buttons
};

// CAN messages for the 5th gen RAM HD platform
const ChryslerAddrs CHRYSLER_RAM_HD_ADDRS = {
  .EPS_2            = 0x220,  // EPS driver input torque
  .ESP_1            = 0x140,  // Brake pedal and vehicle speed
  .ESP_8            = 0x11C,  // Brake pedal and vehicle speed
  .ECM_5            = 0x22F,  // Throttle position sensor
  .DAS_3            = 0x1F4,  // ACC state and control from DASM
  .DAS_4            = 0x1F5,  // ACC and FCW dispaly and config from DASM
  .DAS_5            = 0x271,  // ACC and FCW dispaly and config from DASM
  .DAS_6            = 0x275,  // LKAS HUD and auto headlight control from DASM
  .LKAS_COMMAND     = 0x276,  // LKAS controls from DASM
  .CRUISE_BUTTONS   = 0x23A,  // Cruise control buttons
};

#define CHRYSLER_COMMON_TX_MSGS(addrs, cruise_buttons_bus, lkas_cmd_len)  \
  {(addrs).CRUISE_BUTTONS, (cruise_buttons_bus), 3},                      \
  {(addrs).LKAS_COMMAND, 0, (lkas_cmd_len)},                              \
  {(addrs).DAS_6, 0, 8},                                                  \

#define CHRYSLER_COMMON_LONG_TX_MSGS(addrs)  \
  {(addrs).DAS_3, 0, 8},              \
  {(addrs).DAS_4, 0, 8},              \
  {(addrs).DAS_5, 0, 8},              \

const CanMsg CHRYSLER_TX_MSGS[] = {
  CHRYSLER_COMMON_TX_MSGS(CHRYSLER_ADDRS, 0, 6)
};

const CanMsg CHRYSLER_LONG_TX_MSGS[] = {
  CHRYSLER_COMMON_TX_MSGS(CHRYSLER_ADDRS, 0, 6)
  CHRYSLER_COMMON_LONG_TX_MSGS(CHRYSLER_ADDRS)
};

const CanMsg CHRYSLER_RAM_DT_TX_MSGS[] = {
  CHRYSLER_COMMON_TX_MSGS(CHRYSLER_RAM_DT_ADDRS, 2, 8)
};

const CanMsg CHRYSLER_RAM_DT_LONG_TX_MSGS[] = {
  CHRYSLER_COMMON_TX_MSGS(CHRYSLER_RAM_DT_ADDRS, 2, 8)
  CHRYSLER_COMMON_LONG_TX_MSGS(CHRYSLER_RAM_DT_ADDRS)
};

const CanMsg CHRYSLER_RAM_HD_TX_MSGS[] = {
  CHRYSLER_COMMON_TX_MSGS(CHRYSLER_RAM_HD_ADDRS, 2, 8)
};

const CanMsg CHRYSLER_RAM_HD_LONG_TX_MSGS[] = {
  CHRYSLER_COMMON_TX_MSGS(CHRYSLER_RAM_HD_ADDRS, 2, 8)
  CHRYSLER_COMMON_LONG_TX_MSGS(CHRYSLER_RAM_HD_ADDRS)
};

#define CHRYSLER_COMMON_RX_CHECKS(addrs)                                                                          \
  {.msg = {{(addrs).EPS_2, 0, 8, .check_checksum = true, .max_counter = 15U, .frequency = 100U}, { 0 }, { 0 }}},  \
  {.msg = {{(addrs).ESP_1, 0, 8, .check_checksum = true, .max_counter = 15U, .frequency = 50U}, { 0 }, { 0 }}},   \
  {.msg = {{(addrs).ECM_5, 0, 8, .check_checksum = true, .max_counter = 15U, .frequency = 50U}, { 0 }, { 0 }}},   \

// TODO: use the same message for both (see vehicle_moving below)
#define CHRYSLER_COMMON_ALT_RX_CHECKS()                                                                 \
  {.msg = {{514, 0, 8, .check_checksum = false, .max_counter = 0U, .frequency = 100U}, { 0 }, { 0 }}},  \

#define CHRYSLER_COMMON_RAM_RX_CHECKS(addrs)                                                                    \
  {.msg = {{(addrs).ESP_8, 0, 8, .check_checksum = true, .max_counter = 15U, .frequency = 50U}, { 0 }, { 0 }}}, \

#define CHRYSLER_COMMON_ACC_RX_CHECKS(addrs, das_3_bus)                                                                   \
  {.msg = {{(addrs).DAS_3, (das_3_bus), 8, .check_checksum = true, .max_counter = 15U, .frequency = 50U}, { 0 }, { 0 }}}, \

#define CHRYSLER_COMMON_BUTTONS_RX_CHECKS(addrs)                                                                          \
  {.msg = {{(addrs).CRUISE_BUTTONS, 0, 3, .check_checksum = true, .max_counter = 15U, .frequency = 50U}, { 0 }, { 0 }}},  \

RxCheck chrysler_rx_checks[] = {
  CHRYSLER_COMMON_RX_CHECKS(CHRYSLER_ADDRS)
  CHRYSLER_COMMON_ALT_RX_CHECKS()
  CHRYSLER_COMMON_ACC_RX_CHECKS(CHRYSLER_ADDRS, 0)
};

RxCheck chrysler_long_rx_checks[] = {
  CHRYSLER_COMMON_RX_CHECKS(CHRYSLER_ADDRS)
  CHRYSLER_COMMON_ALT_RX_CHECKS()
  CHRYSLER_COMMON_BUTTONS_RX_CHECKS(CHRYSLER_ADDRS)
};

RxCheck chrysler_ram_dt_rx_checks[] = {
  CHRYSLER_COMMON_RX_CHECKS(CHRYSLER_RAM_DT_ADDRS)
  CHRYSLER_COMMON_RAM_RX_CHECKS(CHRYSLER_RAM_DT_ADDRS)
  CHRYSLER_COMMON_ACC_RX_CHECKS(CHRYSLER_RAM_DT_ADDRS, 2)
};

RxCheck chrysler_ram_dt_long_rx_checks[] = {
  CHRYSLER_COMMON_RX_CHECKS(CHRYSLER_RAM_DT_ADDRS)
  CHRYSLER_COMMON_RAM_RX_CHECKS(CHRYSLER_RAM_DT_ADDRS)
  CHRYSLER_COMMON_BUTTONS_RX_CHECKS(CHRYSLER_RAM_DT_ADDRS)
};

RxCheck chrysler_ram_hd_rx_checks[] = {
  CHRYSLER_COMMON_RX_CHECKS(CHRYSLER_RAM_HD_ADDRS)
  CHRYSLER_COMMON_RAM_RX_CHECKS(CHRYSLER_RAM_HD_ADDRS)
  CHRYSLER_COMMON_ACC_RX_CHECKS(CHRYSLER_RAM_HD_ADDRS, 2)
};

RxCheck chrysler_ram_hd_long_rx_checks[] = {
  CHRYSLER_COMMON_RX_CHECKS(CHRYSLER_RAM_HD_ADDRS)
  CHRYSLER_COMMON_RAM_RX_CHECKS(CHRYSLER_RAM_HD_ADDRS)
  CHRYSLER_COMMON_BUTTONS_RX_CHECKS(CHRYSLER_RAM_HD_ADDRS)
};

const uint32_t CHRYSLER_PARAM_RAM_DT = 1U;  // set for Ram DT platform
const uint32_t CHRYSLER_PARAM_RAM_HD = 2U;  // set for Ram HD platform
const uint32_t CHRYSLER_PARAM_LONGITUDINAL = 4U;

typedef enum {
  CHRYSLER_RAM_DT,
  CHRYSLER_RAM_HD,
  CHRYSLER_PACIFICA,  // plus Jeep
} ChryslerPlatform;
ChryslerPlatform chrysler_platform = CHRYSLER_PACIFICA;
const ChryslerAddrs *chrysler_addrs = &CHRYSLER_ADDRS;

static uint32_t chrysler_get_checksum(const CANPacket_t *to_push) {
  int checksum_byte = GET_LEN(to_push) - 1U;
  return (uint8_t)(GET_BYTE(to_push, checksum_byte));
}

static uint32_t chrysler_compute_checksum(const CANPacket_t *to_push) {
  // TODO: clean this up
  // http://illmatics.com/Remote%20Car%20Hacking.pdf
  uint8_t checksum = 0xFFU;
  int len = GET_LEN(to_push);
  for (int j = 0; j < (len - 1); j++) {
    uint8_t shift = 0x80U;
    uint8_t curr = (uint8_t)GET_BYTE(to_push, j);
    for (int i=0; i<8; i++) {
      uint8_t bit_sum = curr & shift;
      uint8_t temp_chk = checksum & 0x80U;
      if (bit_sum != 0U) {
        bit_sum = 0x1C;
        if (temp_chk != 0U) {
          bit_sum = 1;
        }
        checksum = checksum << 1;
        temp_chk = checksum | 1U;
        bit_sum ^= temp_chk;
      } else {
        if (temp_chk != 0U) {
          bit_sum = 0x1D;
        }
        checksum = checksum << 1;
        bit_sum ^= checksum;
      }
      checksum = bit_sum;
      shift = shift >> 1;
    }
  }
  return (uint8_t)(~checksum);
}

static uint8_t chrysler_get_counter(const CANPacket_t *to_push) {
  int counter_byte = GET_LEN(to_push) - 2U;
  return (uint8_t)(GET_BYTE(to_push, counter_byte) >> 4);
}

static void chrysler_rx_hook(const CANPacket_t *to_push) {
  const int bus = GET_BUS(to_push);
  const int addr = GET_ADDR(to_push);

  if ((bus == 0) && (addr == chrysler_addrs->CRUISE_BUTTONS) && chrysler_longitudinal) {
    int cruise_button = GET_BIT(to_push, 0U);       // cancel button
    // ensure cancel overrides any multi-button pressed state
    if (!cruise_button) {
      cruise_button |= GET_BIT(to_push, 2U) << 2U;  // accel button
      cruise_button |= GET_BIT(to_push, 3U) << 3U;  // decel button
      cruise_button |= GET_BIT(to_push, 4U) << 4U;  // resume button
    }

    // enter controls on falling edge of accel/decel/resume
    bool accel = (cruise_button != CHRYSLER_BTN_ACCEL) && (cruise_button_prev == CHRYSLER_BTN_ACCEL);
    bool decel = (cruise_button != CHRYSLER_BTN_DECEL) && (cruise_button_prev == CHRYSLER_BTN_DECEL);
    bool resume = (cruise_button != CHRYSLER_BTN_RESUME) && (cruise_button_prev == CHRYSLER_BTN_RESUME);
    if (accel || decel || resume) {
      controls_allowed = true;
    }

    // exit controls on cancel press
    if (cruise_button == CHRYSLER_BTN_CANCEL) {
      controls_allowed = false;
    }

    cruise_button_prev = cruise_button;
  }

  // Measured EPS torque
  if ((bus == 0) && (addr == chrysler_addrs->EPS_2)) {
    int torque_meas_new = ((GET_BYTE(to_push, 4) & 0x7U) << 8) + GET_BYTE(to_push, 5) - 1024U;
    update_sample(&torque_meas, torque_meas_new);
  }

  if (!chrysler_longitudinal) {
    // enter controls on rising edge of ACC, exit controls on ACC off
    const int das_3_bus = (chrysler_platform == CHRYSLER_PACIFICA) ? 0 : 2;
    if ((bus == das_3_bus) && (addr == chrysler_addrs->DAS_3)) {
      bool cruise_engaged = GET_BIT(to_push, 21U);
      pcm_cruise_check(cruise_engaged);
    }
  }

  // TODO: use the same message for both
  // update vehicle moving
  if ((chrysler_platform != CHRYSLER_PACIFICA) && (bus == 0) && (addr == chrysler_addrs->ESP_8)) {
    vehicle_moving = ((GET_BYTE(to_push, 4) << 8) + GET_BYTE(to_push, 5)) != 0U;
  }
  if ((chrysler_platform == CHRYSLER_PACIFICA) && (bus == 0) && (addr == 514)) {
    int speed_l = (GET_BYTE(to_push, 0) << 4) + (GET_BYTE(to_push, 1) >> 4);
    int speed_r = (GET_BYTE(to_push, 2) << 4) + (GET_BYTE(to_push, 3) >> 4);
    vehicle_moving = (speed_l != 0) || (speed_r != 0);
  }

  // exit controls on rising edge of gas press
  if ((bus == 0) && (addr == chrysler_addrs->ECM_5)) {
    gas_pressed = GET_BYTE(to_push, 0U) != 0U;
  }

  // exit controls on rising edge of brake press
  if ((bus == 0) && (addr == chrysler_addrs->ESP_1)) {
    brake_pressed = ((GET_BYTE(to_push, 0U) & 0xFU) >> 2U) == 1U;
  }

  generic_rx_checks((bus == 0) && (addr == chrysler_addrs->LKAS_COMMAND));
}

static bool chrysler_tx_hook(const CANPacket_t *to_send) {
  bool tx = true;
  int addr = GET_ADDR(to_send);

  // STEERING
  if (addr == chrysler_addrs->LKAS_COMMAND) {
    int start_byte = (chrysler_platform == CHRYSLER_PACIFICA) ? 0 : 1;
    int desired_torque = ((GET_BYTE(to_send, start_byte) & 0x7U) << 8) | GET_BYTE(to_send, start_byte + 1);
    desired_torque -= 1024;

    const SteeringLimits limits = (chrysler_platform == CHRYSLER_PACIFICA) ? CHRYSLER_STEERING_LIMITS :
                                  (chrysler_platform == CHRYSLER_RAM_DT) ? CHRYSLER_RAM_DT_STEERING_LIMITS : CHRYSLER_RAM_HD_STEERING_LIMITS;

    bool steer_req = (chrysler_platform == CHRYSLER_PACIFICA) ? GET_BIT(to_send, 4U) : (GET_BYTE(to_send, 3) & 0x7U) == 2U;
    if (steer_torque_cmd_checks(desired_torque, steer_req, limits)) {
      tx = false;
    }
  }

  // ACCEL
  if (tx && (addr == chrysler_addrs->DAS_3)) {
    // Signal: ENGINE_TORQUE_REQUEST
    int gas = (((GET_BYTE(to_send, 0) & 0x1FU) << 8) | GET_BYTE(to_send, 1));
    // Signal: ACC_DECEL
    int accel = (((GET_BYTE(to_send, 2) & 0xFU) << 8) | GET_BYTE(to_send, 3));

    bool violation = false;
    violation |= longitudinal_accel_checks(accel, CHRYSLER_LONG_LIMITS);
    violation |= longitudinal_gas_checks(gas, CHRYSLER_LONG_LIMITS);

    if (violation) {
      tx = 0;
    }
  }

  // FORCE CANCEL: only the cancel button press is allowed
  if ((addr == chrysler_addrs->CRUISE_BUTTONS) && !chrysler_longitudinal) {
    const bool is_cancel = GET_BYTE(to_send, 0) == 1U;
    const bool is_resume = GET_BYTE(to_send, 0) == 0x10U;
    const bool allowed = is_cancel || (is_resume && controls_allowed);
    if (!allowed) {
      tx = false;
    }
  }

  return tx;
}

static int chrysler_fwd_hook(int bus_num, int addr) {
  int bus_fwd = -1;

  // forward to camera
  const bool is_buttons = (addr == chrysler_addrs->CRUISE_BUTTONS);
  if ((bus_num == 0) && !(chrysler_longitudinal && is_buttons)) {
    bus_fwd = 2;
  }

  // forward all messages from camera except LKAS messages
  const bool is_lkas = ((addr == chrysler_addrs->LKAS_COMMAND) || (addr == chrysler_addrs->DAS_6));
  const bool is_acc = ((addr == chrysler_addrs->DAS_3) || (addr == chrysler_addrs->DAS_4) || (addr == chrysler_addrs->DAS_5));
  if ((bus_num == 2) && !is_lkas && !(chrysler_longitudinal && is_acc)) {
    bus_fwd = 0;
  }

  return bus_fwd;
}

static safety_config chrysler_init(uint16_t param) {
#ifdef ALLOW_DEBUG
  chrysler_longitudinal = GET_FLAG(param, CHRYSLER_PARAM_LONGITUDINAL);
#else
  chrysler_longitudinal = false;
#endif

  safety_config ret;

  bool enable_ram_dt = GET_FLAG(param, CHRYSLER_PARAM_RAM_DT);
  if (enable_ram_dt) {
    chrysler_platform = CHRYSLER_RAM_DT;
    chrysler_addrs = &CHRYSLER_RAM_DT_ADDRS;
    ret = chrysler_longitudinal ? BUILD_SAFETY_CFG(chrysler_ram_dt_long_rx_checks, CHRYSLER_RAM_DT_LONG_TX_MSGS) : BUILD_SAFETY_CFG(chrysler_ram_dt_rx_checks, CHRYSLER_RAM_DT_TX_MSGS);
#ifdef ALLOW_DEBUG
  } else if (GET_FLAG(param, CHRYSLER_PARAM_RAM_HD)) {
    chrysler_platform = CHRYSLER_RAM_HD;
    chrysler_addrs = &CHRYSLER_RAM_HD_ADDRS;
    ret = chrysler_longitudinal ? BUILD_SAFETY_CFG(chrysler_ram_hd_long_rx_checks, CHRYSLER_RAM_HD_LONG_TX_MSGS) : BUILD_SAFETY_CFG(chrysler_ram_hd_rx_checks, CHRYSLER_RAM_HD_TX_MSGS);
#endif
  } else {
    chrysler_platform = CHRYSLER_PACIFICA;
    chrysler_addrs = &CHRYSLER_ADDRS;
    ret = chrysler_longitudinal ? BUILD_SAFETY_CFG(chrysler_long_rx_checks, CHRYSLER_LONG_TX_MSGS) : BUILD_SAFETY_CFG(chrysler_rx_checks, CHRYSLER_TX_MSGS);
  }
  return ret;
}

const safety_hooks chrysler_hooks = {
  .init = chrysler_init,
  .rx = chrysler_rx_hook,
  .tx = chrysler_tx_hook,
  .fwd = chrysler_fwd_hook,
  .get_counter = chrysler_get_counter,
  .get_checksum = chrysler_get_checksum,
  .compute_checksum = chrysler_compute_checksum,
};
