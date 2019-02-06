const int VW_MAX_STEER = 300;
const int VW_MAX_RT_DELTA = 128;          // max delta torque allowed for real time checks
const int32_t VW_RT_INTERVAL = 250000;    // 250ms between real time checks
const int VW_MAX_RATE_UP = 7;
const int VW_MAX_RATE_DOWN = 17;
const int VW_DRIVER_TORQUE_ALLOWANCE = 50;
const int VW_DRIVER_TORQUE_FACTOR = 4;

int vw_ignition_started = 0;
struct sample_t vw_torque_driver;         // last few driver torques measured
int vw_cruise_engaged_last = 0;
int vw_rt_torque_last = 0;
int vw_desired_torque_last = 0;
uint32_t vw_ts_last = 0;

void vw_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {
  int bus_number = (to_push->RDTR >> 4) & 0xFF;
  uint32_t addr;
  if (to_push->RIR & 4)
  {
    // Extended
    // Not looked at, but have to be separated
    // to avoid address collision
    addr = to_push->RIR >> 3;
  }
  else
  {
    // Normal
    addr = to_push->RIR >> 21;
  }

  if (addr == 0x3c0 && bus_number == 0) {
    uint32_t ign = (to_push->RDLR) & 0x200;
    vw_ignition_started = ign > 0;
  }

  // EPS_01 Driver_Strain
  if (addr == 0x9f) {
    int torque_driver_new = (to_push->RDLR & 0x1f00) | ((to_push->RDLR >> 16) & 0xFF);
    
    // EPS_01 Driver_Strain_VZ
    uint8_t sign = (to_push->RDLR & 0x8000) > 0;
    if (sign == 1) {
      torque_driver_new *= -1;
    }

    // update array of samples
    update_sample(&vw_torque_driver, torque_driver_new);
  }

  // enter controls on rising edge of ACC, exit controls on ACC off
  // ACC_06 ACC_Status_ACC
  if ((addr == 0x122) && (bus_number == 0)) {
    uint8_t acc_status = to_push->RDLR & 0x70;
    // TODO: Get a little more sophisticated with ACC states and transitions later
    int cruise_engaged = acc_status > 2;
    if (cruise_engaged && !vw_cruise_engaged_last) {
      controls_allowed = 1;
    } else if (!cruise_engaged) {
      controls_allowed = 0;
    }
    vw_cruise_engaged_last = cruise_engaged;
  }
}

int vw_ign_hook() {
  return vw_ignition_started;
}

static void vw_init(int16_t param) {
  controls_allowed = 0;
  vw_ignition_started = 0;
}

static int vw_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {

  uint32_t addr;
  if (to_send->RIR & 4) {
    // Extended
    addr = to_send->RIR >> 3;
  } else {
    // Normal
    addr = to_send->RIR >> 21;
  }

  // LKA STEER: safety check
  if (addr == 0x126) {
    int desired_torque = ((to_send->RDHR & 0x3f) << 8) | ((to_send->RDHR >> 8) & 0xFF);

    // LKAS _LKAS_Boost (Should be renamed to Steer_Torque_VZ)
    uint8_t sign = (to_send->RDHR & 0x80) > 0;
    if (sign == 1) {
      desired_torque *= -1;
    }

    uint32_t ts = TIM2->CNT;
    int violation = 0;

    if (controls_allowed) {

      // *** global torque limit check ***
      violation |= max_limit_check(desired_torque, VW_MAX_STEER, -VW_MAX_STEER);

      // *** torque rate limit check ***
      violation |= driver_limit_check(desired_torque, vw_desired_torque_last, &vw_torque_driver,
        VW_MAX_STEER, VW_MAX_RATE_UP, VW_MAX_RATE_DOWN,
        VW_DRIVER_TORQUE_ALLOWANCE, VW_DRIVER_TORQUE_FACTOR);

      // used next time
      vw_desired_torque_last = desired_torque;

      // *** torque real time rate limit check ***
      violation |= rt_rate_limit_check(desired_torque, vw_rt_torque_last, VW_MAX_RT_DELTA);

      // every RT_INTERVAL set the new limits
      uint32_t ts_elapsed = get_ts_elapsed(ts, vw_ts_last);
      if (ts_elapsed > VW_RT_INTERVAL) {
        vw_rt_torque_last = desired_torque;
        vw_ts_last = ts;
      }
    }

    // no torque if controls is not allowed
    if (!controls_allowed && (desired_torque != 0)) {
      violation = 1;
    }

    // reset to 0 if either controls is not allowed or there's a violation
    if (violation || !controls_allowed) {
      vw_desired_torque_last = 0;
      vw_rt_torque_last = 0;
      vw_ts_last = ts;
    }

    if (violation) {
      return false;
    }
  }

  return true;
}

static int vw_tx_lin_hook(int lin_num, uint8_t *data, int len) {
  return true;
}

static int vw_fwd_hook(int bus_num, CAN_FIFOMailBox_TypeDef *to_fwd) {
  // shifts bits from 29 to 11
  int32_t addr = to_fwd->RIR >> 21;

  // forward messages from car to extended
  if (bus_num == 0) {

    return 1; //extended

  }
  // forward messages from extended to car
  else if (bus_num == 1) {

    //filter 0x126 from being forwarded
    if (addr == 0x126) {
      return -1;
    }

    return 0; //car
  }

  // fallback to do not forward
  return -1;
}

const safety_hooks vw_hooks = {
  .init = vw_init,
  .rx = vw_rx_hook,
  .tx = vw_tx_hook,
  .tx_lin = vw_tx_lin_hook,
  .ignition = vw_ign_hook,
  .fwd = vw_fwd_hook,
};
