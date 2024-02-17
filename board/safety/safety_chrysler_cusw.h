const SteeringLimits CHRYSLER_CUSW_STEERING_LIMITS = {
  .max_steer = 261,
  .max_rt_delta = 112,
  .max_rt_interval = 250000,
  .max_rate_up = 3,
  .max_rate_down = 3,
  .max_torque_error = 80,
  .type = TorqueMotorLimited,
};

typedef struct {
  const int BRAKE_1;
  const int BRAKE_2;
  const int EPS_STATUS;
  const int ACCEL_GAS;
  const int LKAS_COMMAND;
  const int CRUISE_BUTTONS;
  const int DAS_6;
  const int ACC_1;
} ChryslerCuswAddrs;

// CAN messages for Chrysler Compact US Wide platforms
const ChryslerCuswAddrs CHRYSLER_CUSW_ADDRS = {
  .BRAKE_1          = 0x1E4,
  .BRAKE_2          = 0x2E2,
  .EPS_STATUS       = 0x1EC,
  .ACCEL_GAS        = 0x1FE,
  .LKAS_COMMAND     = 0x1F6,
  .CRUISE_BUTTONS   = 0x2FA,
  .DAS_6            = 0x5DC,
  .ACC_1            = 0x3EE,
};

const CanMsg CHRYSLER_CUSW_TX_MSGS[] = {
  {CHRYSLER_CUSW_ADDRS.CRUISE_BUTTONS, 0, 8},
  {CHRYSLER_CUSW_ADDRS.LKAS_COMMAND, 0, 4},
  {CHRYSLER_CUSW_ADDRS.DAS_6, 0, 4},
};

RxCheck chrysler_cusw_rx_checks[] = {
// FIXME: these should have checksums and counters but something is broken
  {.msg = {{CHRYSLER_CUSW_ADDRS.BRAKE_1, 0, 8, .check_checksum = false, .max_counter = 0U, .frequency = 50U}, { 0 }, { 0 }}},
  {.msg = {{CHRYSLER_CUSW_ADDRS.BRAKE_2, 0, 8, .check_checksum = false, .max_counter = 0U, .frequency = 50U}, { 0 }, { 0 }}},
  {.msg = {{CHRYSLER_CUSW_ADDRS.EPS_STATUS, 0, 8, .check_checksum = false, .max_counter = 0U, .frequency = 100U}, { 0 }, { 0 }}},
  {.msg = {{CHRYSLER_CUSW_ADDRS.ACCEL_GAS, 0, 5, .check_checksum = false, .max_counter = 0U, .frequency = 50U}, { 0 }, { 0 }}},
// TODO: check to see if this really has no counter/checksum
  {.msg = {{CHRYSLER_CUSW_ADDRS.ACC_1, 0, 8, .check_checksum = false, .max_counter = 0U, .frequency = 17U}, { 0 }, { 0 }}},
};

const ChryslerCuswAddrs *chrysler_cusw_addrs = &CHRYSLER_CUSW_ADDRS;

// TODO: include from main Chrysler safety
static uint32_t chrysler_cusw_get_checksum(const CANPacket_t *to_push) {
  int checksum_byte = GET_LEN(to_push) - 1U;
  return (uint8_t)(GET_BYTE(to_push, checksum_byte));
}

// TODO: include from main Chrysler safety
static uint32_t chrysler_cusw_compute_checksum(const CANPacket_t *to_push) {
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

// TODO: include from main Chrysler safety
static uint8_t chrysler_cusw_get_counter(const CANPacket_t *to_push) {
  return (uint8_t)(GET_BYTE(to_push, 6) >> 4);
}

static void chrysler_cusw_rx_hook(const CANPacket_t *to_push) {
  const int bus = GET_BUS(to_push);
  const int addr = GET_ADDR(to_push);

  // Measured EPS torque
  if ((bus == 0) && (addr == chrysler_cusw_addrs->EPS_STATUS)) {
    int torque_meas_new = ((GET_BYTE(to_push, 3) & 0xFU) << 8) + GET_BYTE(to_push, 4) - 2048U;
    update_sample(&torque_meas, torque_meas_new);
  }

  // enter controls on rising edge of ACC, exit controls on ACC off
  if ((bus == 0) && (addr == chrysler_cusw_addrs->ACC_1)) {
    uint8_t acc_state = (GET_BYTE(to_push, 0) & 0xE0U) >> 5;
    bool cruise_engaged = acc_state == 4U;
    pcm_cruise_check(cruise_engaged);
  }

  // TODO: use the same message for both
  // update vehicle moving
  if ((bus == 0) && (addr == chrysler_cusw_addrs->BRAKE_1)) {
    vehicle_moving = (((GET_BYTE(to_push, 4) & 0x7U) << 8) + GET_BYTE(to_push, 5)) != 0U;
  }

  // exit controls on rising edge of gas press
  if ((bus == 0) && (addr == chrysler_cusw_addrs->ACCEL_GAS)) {
    gas_pressed = GET_BYTE(to_push, 1U) != 0U;
  }

  // exit controls on rising edge of brake press
  if ((bus == 0) && (addr == chrysler_cusw_addrs->BRAKE_2)) {
    brake_pressed = GET_BIT(to_push, 9U);
  }

  generic_rx_checks((bus == 0) && (addr == chrysler_cusw_addrs->LKAS_COMMAND));
}

static bool chrysler_cusw_tx_hook(const CANPacket_t *to_send) {
  bool tx = true;
  int addr = GET_ADDR(to_send);

  // STEERING
  if (addr == chrysler_cusw_addrs->LKAS_COMMAND) {
    int desired_torque = ((GET_BYTE(to_send, 0)) << 3) | ((GET_BYTE(to_send, 1) & 0xE0U) >> 5);
    desired_torque -= 1024;

    const SteeringLimits limits = CHRYSLER_CUSW_STEERING_LIMITS;

    bool steer_req = GET_BIT(to_send, 12) != 0U;
    if (steer_torque_cmd_checks(desired_torque, steer_req, limits)) {
      tx = false;
    }
  }

  // FORCE CANCEL: only the cancel button press is allowed
  if (addr == chrysler_cusw_addrs->CRUISE_BUTTONS) {
    const bool is_cancel = GET_BIT(to_send, 0) != 0U;
    const bool is_resume = GET_BIT(to_send, 4) != 0U;
    const bool allowed = is_cancel || (is_resume && controls_allowed);
    if (!allowed) {
      tx = false;
    }
  }

  return tx;
}

static int chrysler_cusw_fwd_hook(int bus_num, int addr) {
  int bus_fwd = -1;

  // forward to camera
  if (bus_num == 0) {
    bus_fwd = 2;
  }

  // forward all messages from camera except LKAS messages
  const bool is_lkas = ((addr == chrysler_cusw_addrs->LKAS_COMMAND) || (addr == chrysler_cusw_addrs->DAS_6));
  if ((bus_num == 2) && !is_lkas){
    bus_fwd = 0;
  }

  return bus_fwd;
}

static safety_config chrysler_cusw_init(uint16_t param) {
  UNUSED(param);
  chrysler_cusw_addrs = &CHRYSLER_CUSW_ADDRS;
  return BUILD_SAFETY_CFG(chrysler_cusw_rx_checks, CHRYSLER_CUSW_TX_MSGS);
}

const safety_hooks chrysler_cusw_hooks = {
  .init = chrysler_cusw_init,
  .rx = chrysler_cusw_rx_hook,
  .tx = chrysler_cusw_tx_hook,
  .fwd = chrysler_cusw_fwd_hook,
  .get_counter = chrysler_cusw_get_counter,
  .get_checksum = chrysler_cusw_get_checksum,
  .compute_checksum = chrysler_cusw_compute_checksum,
};
