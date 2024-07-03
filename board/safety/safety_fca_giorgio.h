// lateral limits
const SteeringLimits FCA_GIORGIO_STEERING_LIMITS = {
  .max_steer = 300,
  .max_rt_delta = 150,
  .max_rt_interval = 250000,
  .max_rate_up = 4,
  .max_rate_down = 4,
  .driver_torque_allowance = 80,
  .driver_torque_factor = 3,
  .type = TorqueDriverLimited,
};

#define FCA_GIORGIO_ABS_1           0xEE
#define FCA_GIORGIO_ABS_3           0xFA
#define FCA_GIORGIO_EPS_3           0x122
#define FCA_GIORGIO_LKA_COMMAND     0x1F6
#define FCA_GIORGIO_LKA_HUD_1       0x4AE
#define FCA_GIORGIO_LKA_HUD_2       0x547
#define FCA_GIORGIO_ACC_1           0x5A2

// TODO: need to find a button message for cancel spam
const CanMsg FCA_GIORGIO_TX_MSGS[] = {{FCA_GIORGIO_LKA_COMMAND, 0, 8}, {FCA_GIORGIO_LKA_HUD_1, 0, 8}, {FCA_GIORGIO_LKA_HUD_2, 0, 8}};

// TODO: need to find a message for driver gas
// TODO: re-check counter/checksum for ABS_3
// TODO: reenable checksums/counters on ABS_1 and EPS_3 once checksums are bruteforced
RxCheck fca_giorgio_rx_checks[] = {
  {.msg = {{FCA_GIORGIO_ACC_1, 0, 8, .check_checksum = false, .max_counter = 0U, .frequency = 12U}, { 0 }, { 0 }}},
  {.msg = {{FCA_GIORGIO_ABS_1, 0, 8, .check_checksum = false, .max_counter = 0U, .frequency = 100U}, { 0 }, { 0 }}},
  {.msg = {{FCA_GIORGIO_ABS_3, 0, 8, .check_checksum = false, .max_counter = 0U, .frequency = 100U}, { 0 }, { 0 }}},
  {.msg = {{FCA_GIORGIO_EPS_3, 0, 4, .check_checksum = false, .max_counter = 0U, .frequency = 100U}, { 0 }, { 0 }}},
};

uint8_t fca_giorgio_crc8_lut_j1850[256];  // Static lookup table for CRC8 SAE J1850

static uint32_t fca_giorgio_get_checksum(const CANPacket_t *to_push) {
  int checksum_byte = GET_LEN(to_push) - 1U;
  return (uint8_t)(GET_BYTE(to_push, checksum_byte));
}

static uint8_t fca_giorgio_get_counter(const CANPacket_t *to_push) {
  int counter_byte = GET_LEN(to_push) - 2U;
  return (uint8_t)(GET_BYTE(to_push, counter_byte) & 0xFU);
}

static uint32_t fca_giorgio_compute_crc(const CANPacket_t *to_push) {
  int addr = GET_ADDR(to_push);
  int len = GET_LEN(to_push);

  // CRC is in the last byte, poly is same as SAE J1850 but uses a different init value and output XOR
  uint8_t crc = 0U;
  uint8_t final_xor = 0U;

  for (int i = 0; i < len - 1; i++) {
    crc ^= (uint8_t)GET_BYTE(to_push, i);
    crc = fca_giorgio_crc8_lut_j1850[crc];
  }

  // TODO: bruteforce final XORs for Panda relevant messages
  if (addr == 0xFFU) {
    final_xor = 0xFFU;
  }

  return (uint8_t)(crc ^ final_xor);
}

static safety_config fca_giorgio_init(uint16_t param) {
  UNUSED(param);

  gen_crc_lookup_table_8(0x2F, fca_giorgio_crc8_lut_j1850);
  return BUILD_SAFETY_CFG(fca_giorgio_rx_checks, FCA_GIORGIO_TX_MSGS);
}

static void fca_giorgio_rx_hook(const CANPacket_t *to_push) {
  if (GET_BUS(to_push) == 0U) {
    int addr = GET_ADDR(to_push);

    // Update in-motion state by sampling wheel speeds
    if (addr == FCA_GIORGIO_ABS_1) {
      // Thanks, FCA, for these 13 bit signals. Makes perfect sense. Great work.
      // Signals: ABS_3.WHEEL_SPEED_[FL,FR,RL,RR]
      int wheel_speed_fl = (GET_BYTE(to_push, 1) >> 3) | (GET_BYTE(to_push, 0) << 5);
      int wheel_speed_fr = (GET_BYTE(to_push, 3) >> 6) | (GET_BYTE(to_push, 2) << 2) | ((GET_BYTE(to_push, 1) & 0x7U) << 10);
      int wheel_speed_rl = (GET_BYTE(to_push, 4) >> 1) | ((GET_BYTE(to_push, 3) & 0x3FU) << 7);
      int wheel_speed_rr = (GET_BYTE(to_push, 6) >> 4) | (GET_BYTE(to_push, 5) << 4) | ((GET_BYTE(to_push, 4) & 0x1U) << 12);
      vehicle_moving = (wheel_speed_fl + wheel_speed_fr + wheel_speed_rl + wheel_speed_rr) > 0;
    }

    // Update driver input torque samples
    // Signal: EPS_3.EPS_TORQUE
    if (addr == FCA_GIORGIO_EPS_3) {
      int torque_driver_new = ((GET_BYTE(to_push, 1) >> 4) | (GET_BYTE(to_push, 0) << 4)) - 2048U;
      update_sample(&torque_driver, torque_driver_new);
    }

    if (addr == FCA_GIORGIO_ACC_1) {
      // When using stock ACC, enter controls on rising edge of stock ACC engage, exit on disengage
      // Always exit controls on main switch off
      // Signal: ACC_1.CRUISE_STATUS
      int acc_status = (GET_BYTE(to_push, 2) & 0x60U) >> 5;
      bool cruise_engaged = (acc_status == 2) || (acc_status == 3);
      acc_main_on = cruise_engaged || (acc_status == 1);

      pcm_cruise_check(cruise_engaged);

      if (!acc_main_on) {
        controls_allowed = false;
      }
    }

    // TODO: find cruise button message

    // TODO: find a driver gas message

    // Signal: ABS_3.BRAKE_PEDAL_SWITCH
    if (addr == FCA_GIORGIO_ABS_3) {
      brake_pressed = GET_BIT(to_push, 3);
    }

    generic_rx_checks((addr == FCA_GIORGIO_LKA_COMMAND));
  }
}

static bool fca_giorgio_tx_hook(const CANPacket_t *to_send) {
  int addr = GET_ADDR(to_send);
  bool tx = true;

  // Safety check for HCA_01 Heading Control Assist torque
  // Signal: LKA_COMMAND.
  // Signal: HCA_01.HCA_01_LM_OffSign (direction)
  if (addr == FCA_GIORGIO_LKA_COMMAND) {
    int desired_torque = ((GET_BYTE(to_send, 1) >> 5) | (GET_BYTE(to_send, 0) << 8)) - 1024U;
    bool steer_req = GET_BIT(to_send, 11U);

    if (steer_torque_cmd_checks(desired_torque, steer_req, FCA_GIORGIO_STEERING_LIMITS)) {
      tx = false;
    }
  }

  // TODO: sanity check cancel spam, once a button message is found

  // FIXME: don't actually run any checks during early testing
  tx = true;

  return tx;
}

static int fca_giorgio_fwd_hook(int bus_num, int addr) {
  int bus_fwd = -1;

  switch (bus_num) {
    case 0:
      bus_fwd = 2;
      break;
    case 2:
      if ((addr == FCA_GIORGIO_LKA_COMMAND) || (addr == FCA_GIORGIO_LKA_HUD_1) || (addr == FCA_GIORGIO_LKA_HUD_2)) {
        bus_fwd = -1;
      } else {
        bus_fwd = 0;
      }
      break;
    default:
      bus_fwd = -1;
      break;
  }

  return bus_fwd;
}

const safety_hooks fca_giorgio_hooks = {
  .init = fca_giorgio_init,
  .rx = fca_giorgio_rx_hook,
  .tx = fca_giorgio_tx_hook,
  .fwd = fca_giorgio_fwd_hook,
  .get_counter = fca_giorgio_get_counter,
  .get_checksum = fca_giorgio_get_checksum,
  .compute_checksum = fca_giorgio_compute_crc,
};
