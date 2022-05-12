const int HYUNDAI_HDA2_MAX_STEER = 384;             // like stock
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
  {.msg = {{53, 1, 32, .check_checksum = false, .max_counter = 0U, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{101, 1, 32, .check_checksum = false, .max_counter = 0U, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{373, 1, 24, .check_checksum = false, .max_counter = 0U, .expected_timestep = 10000U}, { 0 }, { 0 }}},
};
#define HYUNDAI_HDA2_ADDR_CHECK_LEN (sizeof(hyundai_hda2_addr_checks) / sizeof(hyundai_hda2_addr_checks[0]))

addr_checks hyundai_hda2_rx_checks = {hyundai_hda2_addr_checks, HYUNDAI_HDA2_ADDR_CHECK_LEN};

static int hyundai_hda2_rx_hook(CANPacket_t *to_push) {

  bool valid = addr_safety_check(to_push, &hyundai_hda2_rx_checks, NULL, NULL, NULL);

  if (valid && (GET_BUS(to_push) == 1U)) {
    int addr = GET_ADDR(to_push);

    if (addr == 373) {
      bool cruise_engaged = GET_BIT(to_push, 68);

      if (cruise_engaged && !cruise_engaged_prev) {
        controls_allowed = 1;
      }

      if (!cruise_engaged) {
        controls_allowed = 0;
      }
      cruise_engaged_prev = cruise_engaged;
    }

    if (addr == 53) {
      gas_pressed = GET_BYTE(to_push, 5) != 0U;
    }

    if (addr == 101) {
      brake_pressed = GET_BIT(to_push, 57) != 0U;
    }

    generic_rx_checks(addr == 0x50);
  }
  return valid;
}

static int hyundai_hda2_tx_hook(CANPacket_t *to_send, bool longitudinal_allowed) {
  UNUSED(longitudinal_allowed);

  int tx = msg_allowed(to_send, HYUNDAI_HDA2_TX_MSGS, sizeof(HYUNDAI_HDA2_TX_MSGS)/sizeof(HYUNDAI_HDA2_TX_MSGS[0]));
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
