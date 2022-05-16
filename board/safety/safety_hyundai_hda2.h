const int HYUNDAI_HDA2_MAX_STEER = 150;
const int HYUNDAI_HDA2_MAX_RT_DELTA = 112;          // max delta torque allowed for real time checks
const uint32_t HYUNDAI_HDA2_RT_INTERVAL = 250000;   // 250ms between real time checks
const int HYUNDAI_HDA2_MAX_RATE_UP = 3;
const int HYUNDAI_HDA2_MAX_RATE_DOWN = 7;
const int HYUNDAI_HDA2_DRIVER_TORQUE_ALLOWANCE = 50;
const int HYUNDAI_HDA2_DRIVER_TORQUE_FACTOR = 2;
const int HYUNDAI_HDA2_STANDSTILL_THRSLD = 30;  // ~1kph

const CanMsg HYUNDAI_HDA2_TX_MSGS[] = {
  {0x50, 0, 16},
  {0x1CF, 1, 8},
};

AddrCheckStruct hyundai_hda2_addr_checks[] = {
  {.msg = {{0x35, 1, 32, .check_checksum = false, .max_counter = 0xffU, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{0x65, 1, 32, .check_checksum = false, .max_counter = 0xffU, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{0xa0, 1, 24, .check_checksum = false, .max_counter = 0xffU, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{0x175, 1, 24, .check_checksum = false, .max_counter = 0xffU, .expected_timestep = 10000U}, { 0 }, { 0 }}},
};
#define HYUNDAI_HDA2_ADDR_CHECK_LEN (sizeof(hyundai_hda2_addr_checks) / sizeof(hyundai_hda2_addr_checks[0]))

addr_checks hyundai_hda2_rx_checks = {hyundai_hda2_addr_checks, HYUNDAI_HDA2_ADDR_CHECK_LEN};

static uint8_t hyundai_hda2_get_counter(CANPacket_t *to_push) {
  return GET_BYTE(to_push, 2);
}

static uint32_t hyundai_hda2_get_checksum(CANPacket_t *to_push) {
  uint32_t chksum = GET_BYTE(to_push, 0) | (GET_BYTE(to_push, 1) << 8);
  return chksum;
}

static uint32_t hyundai_hda2_compute_checksum(CANPacket_t *to_push) {
  uint32_t chksum = GET_BYTE(to_push, 0) | (GET_BYTE(to_push, 1) << 8);
  return chksum;
}

static int hyundai_hda2_rx_hook(CANPacket_t *to_push) {

  bool valid = addr_safety_check(to_push, &hyundai_hda2_rx_checks,
                                 hyundai_hda2_get_checksum, hyundai_hda2_compute_checksum, hyundai_hda2_get_counter);

  if (valid && (GET_BUS(to_push) == 1U)) {
    int addr = GET_ADDR(to_push);

    if (addr == 0x175) {
      bool cruise_engaged = GET_BIT(to_push, 68);

      if (cruise_engaged && !cruise_engaged_prev) {
        controls_allowed = 1;
      }

      if (!cruise_engaged) {
        controls_allowed = 0;
      }
      cruise_engaged_prev = cruise_engaged;
    }

    if (addr == 0x35) {
      gas_pressed = GET_BYTE(to_push, 5) != 0U;
    }

    if (addr == 0x64) {
      brake_pressed = GET_BIT(to_push, 57) != 0U;
    }

    if (addr == 0xa0) {
      int speed = 0;
      for (int i = 8; i < 15; i+=2) {
        speed += GET_BYTE(to_push, i) | (GET_BYTE(to_push, i + 1) << 8);
      }
      speed = ABS(speed) / 4;
      vehicle_moving = speed > HYUNDAI_HDA2_STANDSTILL_THRSLD;
    }

    generic_rx_checks(addr == 0x50);
  }
  return valid;
}

static int hyundai_hda2_tx_hook(CANPacket_t *to_send, bool longitudinal_allowed) {
  UNUSED(longitudinal_allowed);

  int tx = msg_allowed(to_send, HYUNDAI_HDA2_TX_MSGS, sizeof(HYUNDAI_HDA2_TX_MSGS)/sizeof(HYUNDAI_HDA2_TX_MSGS[0]));
  int addr = GET_ADDR(to_send);
  int bus = GET_BUS(to_send);

  // steering
  if ((addr == 0x50) && (bus == 0)) {
    int desired_torque = (((GET_BYTE(to_send, 6) & 0xF) << 7) | (GET_BYTE(to_send, 5) >> 1)) - 1024U;
    uint32_t ts = microsecond_timer_get();
    bool violation = 0;

    if (controls_allowed) {

      // *** global torque limit check ***
      violation |= max_limit_check(desired_torque, HYUNDAI_HDA2_MAX_STEER, -HYUNDAI_HDA2_MAX_STEER);

      /*
      // *** torque rate limit check ***
      violation |= driver_limit_check(desired_torque, desired_torque_last, &torque_driver,
        HYUNDAI_MAX_STEER, HYUNDAI_MAX_RATE_UP, HYUNDAI_MAX_RATE_DOWN,
        HYUNDAI_DRIVER_TORQUE_ALLOWANCE, HYUNDAI_DRIVER_TORQUE_FACTOR);
      */

      // used next time
      desired_torque_last = desired_torque;

      // *** torque real time rate limit check ***
      violation |= rt_rate_limit_check(desired_torque, rt_torque_last, HYUNDAI_MAX_RT_DELTA);

      // every RT_INTERVAL set the new limits
      uint32_t ts_elapsed = get_ts_elapsed(ts, ts_last);
      if (ts_elapsed > HYUNDAI_RT_INTERVAL) {
        rt_torque_last = desired_torque;
        ts_last = ts;
      }
    }

    // no torque if controls is not allowed
    if (!controls_allowed && (desired_torque != 0)) {
      violation = 1;
    }

    // reset to 0 if either controls is not allowed or there's a violation
    if (violation || !controls_allowed) {
      desired_torque_last = 0;
      rt_torque_last = 0;
      ts_last = ts;
    }

    if (violation) {
      tx = 0;
    }
  }

  // cruise buttons check
  if ((addr == 0x1CF) && (bus == 1)) {
    bool is_cancel = (GET_BYTES_04(to_send) == (1U << 18)) && (GET_BYTES_48(to_send) == 0U);
    if (!is_cancel || !cruise_engaged_prev) {
      tx = 0;
    }
  }


  return tx;
}

static int hyundai_hda2_fwd_hook(int bus_num, CANPacket_t *to_fwd) {

  int bus_fwd = -1;
  int addr = GET_ADDR(to_fwd);

  if (bus_num == 0) {
    bus_fwd = 2;
  }
  if ((bus_num == 2) && (addr != 0x50)) {
    bus_fwd = 0;
  }

  return bus_fwd;
}

static const addr_checks* hyundai_hda2_init(uint16_t param) {
  UNUSED(param);
  controls_allowed = false;
  relay_malfunction_reset();
  return &hyundai_hda2_rx_checks;
}

const safety_hooks hyundai_hda2_hooks = {
  .init = hyundai_hda2_init,
  .rx = hyundai_hda2_rx_hook,
  .tx = hyundai_hda2_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = hyundai_hda2_fwd_hook,
};
