const SteeringLimits HYUNDAI_CANFD_STEERING_LIMITS = {
  .max_steer = 270,
  .max_rt_delta = 112,
  .max_rt_interval = 250000,
  .max_rate_up = 3,
  .max_rate_down = 7,
  .driver_torque_allowance = 250,
  .driver_torque_factor = 2,
  .type = TorqueDriverLimited,
};

const uint32_t HYUNDAI_CANFD_STANDSTILL_THRSLD = 30;  // ~1kph

const CanMsg HYUNDAI_CANFD_TX_MSGS[] = {
  {0x50, 0, 16},
  {0x1CF, 1, 8},
  {0x2A4, 0, 24},
};

const CanMsg HYUNDAI_TUCSON_HEV_2022_TX_MSGS[] = {
  {0x12a, 0, 16},
  {0x1aa, 0, 16},
};

AddrCheckStruct hyundai_canfd_addr_checks[] = {
  {.msg = {{0x35, 1, 32, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{0x65, 1, 32, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{0xa0, 1, 24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{0xea, 1, 24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{0x175, 1, 24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{0x1cf, 1, 8, .check_checksum = false, .max_counter = 0xfU, .expected_timestep = 20000U}, { 0 }, { 0 }}},
};
#define HYUNDAI_CANFD_ADDR_CHECK_LEN (sizeof(hyundai_canfd_addr_checks) / sizeof(hyundai_canfd_addr_checks[0]))

AddrCheckStruct hyundai_tucson_hev_2022_addr_checks[] = {
  {.msg = {{0x105, 0, 32, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{0x65, 0, 32, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{0xa0, 0, 24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{0xea, 0, 24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{0x175, 0, 24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 20000U}, { 0 }, { 0 }}},
  {.msg = {{0x1aa, 0, 16, .check_checksum = false, .max_counter = 0xffU, .expected_timestep = 20000U}, { 0 }, { 0 }}},
};
#define HYUNDAI_TUCSON_HEV_2022_ADDR_CHECK_LEN (sizeof(hyundai_tucson_hev_2022_addr_checks) / sizeof(hyundai_tucson_hev_2022_addr_checks[0]))

addr_checks hyundai_canfd_rx_checks = {hyundai_canfd_addr_checks, HYUNDAI_CANFD_ADDR_CHECK_LEN};


uint16_t hyundai_canfd_crc_lut[256];

static uint8_t hyundai_canfd_get_counter(CANPacket_t *to_push) {
  uint8_t ret = 0;
  if (GET_LEN(to_push) == 8U) {
    ret = GET_BYTE(to_push, 1) >> 4;
  } else {
    ret = GET_BYTE(to_push, 2);
  }
  return ret;
}

static uint32_t hyundai_canfd_get_checksum(CANPacket_t *to_push) {
  uint32_t chksum = GET_BYTE(to_push, 0) | (GET_BYTE(to_push, 1) << 8);
  return chksum;
}

static uint32_t hyundai_canfd_compute_checksum(CANPacket_t *to_push) {
  int len = GET_LEN(to_push);
  uint32_t address = GET_ADDR(to_push);

  uint16_t crc = 0;

  for (int i = 2; i < len; i++) {
    crc = (crc << 8U) ^ hyundai_canfd_crc_lut[(crc >> 8U) ^ GET_BYTE(to_push, i)];
  }

  // Add address to crc
  crc = (crc << 8U) ^ hyundai_canfd_crc_lut[(crc >> 8U) ^ ((address >> 0U) & 0xFFU)];
  crc = (crc << 8U) ^ hyundai_canfd_crc_lut[(crc >> 8U) ^ ((address >> 8U) & 0xFFU)];

  if (len == 8) {
    crc ^= 0x5f29U;
  } else if (len == 16) {
    crc ^= 0x041dU;
  } else if (len == 24) {
    crc ^= 0x819dU;
  } else if (len == 32) {
    crc ^= 0x9f5bU;
  } else {

  }

  return crc;
}

static int hyundai_canfd_rx_hook(CANPacket_t *to_push) {

  bool valid = addr_safety_check(to_push, &hyundai_canfd_rx_checks,
                                 hyundai_canfd_get_checksum, hyundai_canfd_compute_checksum, hyundai_canfd_get_counter);

  int bus = GET_BUS(to_push);
  int addr = GET_ADDR(to_push);

  if (valid && (bus == 0) && hyundai_tucson_hev_2022) {

    // cruise buttons
    if (addr == 0x1aa) {
      int cruise_button = (GET_BYTE(to_push, 4) >> 4) & 0x7U;
      int main_button = GET_BIT(to_push, 34U);

      if ((cruise_button == HYUNDAI_BTN_RESUME) || (cruise_button == HYUNDAI_BTN_SET) || (cruise_button == HYUNDAI_BTN_CANCEL) || (main_button != 0)) {
        hyundai_last_button_interaction = 0U;
      } else {
        hyundai_last_button_interaction = MIN(hyundai_last_button_interaction + 1U, HYUNDAI_PREV_BUTTON_SAMPLES);
      }
    }

    // gas press
    if (addr == 0x105) {
      gas_pressed = ((GET_BIT(to_push, 103U) != 0U) | (GET_BYTE(to_push, 13) != 0U) | (GET_BIT(to_push, 112U) != 0U));
    }
  }

  if (valid && (bus == 1) && !hyundai_tucson_hev_2022) {

    // cruise buttons
    if (addr == 0x1cf) {
      int cruise_button = GET_BYTE(to_push, 2) & 0x7U;
      int main_button = GET_BIT(to_push, 19U);

      if ((cruise_button == HYUNDAI_BTN_RESUME) || (cruise_button == HYUNDAI_BTN_SET) || (cruise_button == HYUNDAI_BTN_CANCEL) || (main_button != 0)) {
        hyundai_last_button_interaction = 0U;
      } else {
        hyundai_last_button_interaction = MIN(hyundai_last_button_interaction + 1U, HYUNDAI_PREV_BUTTON_SAMPLES);
      }
    }

    // gas press
    if (addr == 0x35) {
      gas_pressed = GET_BYTE(to_push, 5) != 0U;
    }
  }

  if (valid && ((bus == 0) || (bus == 1))) {

    // driver torque
    if (addr == 0xea) {
      int torque_driver_new = ((GET_BYTE(to_push, 11) & 0x1fU) << 8U) | GET_BYTE(to_push, 10);
      torque_driver_new -= 4095;
      update_sample(&torque_driver, torque_driver_new);
    }

    // cruise state
    if (addr == 0x175) {
      bool cruise_engaged = GET_BIT(to_push, 68U);
      if (cruise_engaged && !cruise_engaged_prev && (hyundai_last_button_interaction < HYUNDAI_PREV_BUTTON_SAMPLES)) {
        controls_allowed = 1;
      }

      if (!cruise_engaged) {
        controls_allowed = 0;
      }
      cruise_engaged_prev = cruise_engaged;
    }

    // brake press
    if (addr == 0x65) {
      brake_pressed = GET_BIT(to_push, 57U) != 0U;
    }

    // vehicle moving
    if (addr == 0xa0) {
      uint32_t speed = 0;
      for (int i = 8; i < 15; i+=2) {
        speed += GET_BYTE(to_push, i) | (GET_BYTE(to_push, i + 1) << 8U);
      }
      vehicle_moving = (speed / 4U) > HYUNDAI_CANFD_STANDSTILL_THRSLD;
    }
  }

  bool stock_ecu_detected = false;
  if (hyundai_tucson_hev_2022) {
    stock_ecu_detected = ((addr == 0x12a) && (bus == 0));
  } else {
    stock_ecu_detected = ((addr == 0x50) && (bus == 0));
  }

  generic_rx_checks(stock_ecu_detected);

  return valid;
}

static int hyundai_canfd_tx_hook(CANPacket_t *to_send, bool longitudinal_allowed) {
  UNUSED(longitudinal_allowed);

  int tx = 1;
  int addr = GET_ADDR(to_send);
  int bus = GET_BUS(to_send);

  if (hyundai_tucson_hev_2022) {
    tx = msg_allowed(to_send, HYUNDAI_TUCSON_HEV_2022_TX_MSGS, sizeof(HYUNDAI_TUCSON_HEV_2022_TX_MSGS)/sizeof(HYUNDAI_TUCSON_HEV_2022_TX_MSGS[0]));
  } else {
    tx = msg_allowed(to_send, HYUNDAI_CANFD_TX_MSGS, sizeof(HYUNDAI_CANFD_TX_MSGS)/sizeof(HYUNDAI_CANFD_TX_MSGS[0]));
  }

  // steering
  if (((addr == 0x50) || ((addr == 0x12a) && hyundai_tucson_hev_2022)) && (bus == 0)) {
    int desired_torque = ((GET_BYTE(to_send, 6) & 0xFU) << 7U) | (GET_BYTE(to_send, 5) >> 1U);
    desired_torque -= 1024;

    if (steer_torque_cmd_checks(desired_torque, -1, HYUNDAI_CANFD_STEERING_LIMITS)) {
      tx = 0;
    }
  }

  // cruise buttons check
  if (((addr == 0x1aa) && (bus == 0)) || ((addr == 0x1cf) && (bus == 1))) {
    int button = 0;

    if ((addr == 0x1aa) && (bus == 0) && hyundai_tucson_hev_2022) {
      button = (GET_BYTE(to_send, 4) >> 4) & 0x7U;
    } else if ((addr == 0x1cf) && (bus == 1) && !hyundai_tucson_hev_2022) {
      button = GET_BYTE(to_send, 2) & 0x7U;
    }

    bool is_cancel = (button == 4);
    bool is_resume = (button == 1);
    bool allowed = (is_cancel && cruise_engaged_prev) || (is_resume && controls_allowed);
    if (!allowed) {
      tx = 0;
    }
  }

  return tx;
}

static int hyundai_canfd_fwd_hook(int bus_num, CANPacket_t *to_fwd) {

  int bus_fwd = -1;
  int addr = GET_ADDR(to_fwd);
  int block_msg = 0;

  if (bus_num == 0) {
    bus_fwd = 2;
  }
  if (bus_num == 2) {
    if (hyundai_tucson_hev_2022) {
      block_msg = (addr == 0x12a);
    } else {
      block_msg = ((addr == 0x50) || (addr == 0x2a4));
    }
    if (!block_msg) {
      bus_fwd = 0;
    }
  }

  return bus_fwd;
}

static const addr_checks* hyundai_canfd_init(uint16_t param) {
  gen_crc_lookup_table_16(0x1021, hyundai_canfd_crc_lut);
  hyundai_last_button_interaction = HYUNDAI_PREV_BUTTON_SAMPLES;
  hyundai_tucson_hev_2022 = GET_FLAG(param, HYUNDAI_PARAM_TUCSON_HEV_2022);

  if (hyundai_tucson_hev_2022) {
    hyundai_canfd_rx_checks = (addr_checks){hyundai_tucson_hev_2022_addr_checks, HYUNDAI_TUCSON_HEV_2022_ADDR_CHECK_LEN};
  } else {
    hyundai_canfd_rx_checks = (addr_checks){hyundai_canfd_addr_checks, HYUNDAI_CANFD_ADDR_CHECK_LEN};
  }

  return &hyundai_canfd_rx_checks;
}

const safety_hooks hyundai_canfd_hooks = {
  .init = hyundai_canfd_init,
  .rx = hyundai_canfd_rx_hook,
  .tx = hyundai_canfd_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = hyundai_canfd_fwd_hook,
};
