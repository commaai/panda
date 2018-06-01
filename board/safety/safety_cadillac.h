const int CADILLAC_STEER_MAX = 150; // 1s
const int CADILLAC_IGNITION_TIMEOUT = 1000000; // 1s
// real time torque limit to prevent controls spamming
// the real time limit is 1500/sec
const int32_t CADILLAC_MAX_RT_DELTA = 75;       // max delta torque allowed for real time checks
const int32_t CADILLAC_RT_INTERVAL = 250000;    // 250ms between real time checks

int cadillac_ign = 0;
int cadillac_cruise_engaged_last = 0;
uint32_t cadillac_ts_ign_last = 0;
int cadillac_rt_torque_last = 0;

struct sample_t cadillac_torque_driver;         // last 3 driver torques measured

static void cadillac_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {
  int bus_number = (to_push->RDTR >> 4) & 0xFF;
  uint32_t addr = to_push->RIR >> 21;

  if (addr == 356) {
    int torque_driver_new = ((to_push->RDLR & 0x3) << 8) | ((to_push->RDLR >> 8) & 0xFF);
    torque_driver_new = to_signed(torque_driver_new, 11);

    // update array of sample
    update_sample(&cadillac_torque_driver, torque_driver_new);
  }


  // this message isn't all zeros when ignition is on
  if ((addr == 0x160) && (bus_number == 0) && to_push->RDLR) {
    cadillac_ign = 1;
    cadillac_ts_ign_last = TIM2->CNT; // reset timer when ign is received
  }

  // enter controls on rising edge of ACC, exit controls on ACC off
  if ((addr == 0x370) && (bus_number == 0)) {
    int cruise_engaged = to_push->RDLR & 0x800000;  // bit 23
    if (cruise_engaged && !cadillac_cruise_engaged_last) {
      controls_allowed = 1;
    } else if (!cruise_engaged) {
      controls_allowed = 0;
    }
    cadillac_cruise_engaged_last = cruise_engaged;
  }

  

}

static int cadillac_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {
  uint32_t addr = to_send->RIR >> 21;

  // block steering cmd above 150
  if (addr == 0x151 || addr == 0x152 || addr == 0x153 || addr == 0x154) {
    int desired_torque = ((to_send->RDLR & 0x3f) << 8) + ((to_send->RDLR & 0xff00) >> 8);
    int violation = 0;
    uint32_t ts = TIM2->CNT;
    desired_torque = to_signed(desired_torque, 14);

    if (controls_allowed) {

      // *** global torque limit check ***
      if ((desired_torque > CADILLAC_STEER_MAX) || (desired_torque < -CADILLAC_STEER_MAX)) {
        violation = 1;
      }

      // *** torque real time rate limit check ***
      int16_t highest_rt_torque = max(cadillac_rt_torque_last, 0) + CADILLAC_MAX_RT_DELTA;
      int16_t lowest_rt_torque = min(cadillac_rt_torque_last, 0) - CADILLAC_MAX_RT_DELTA;

      // check for violation
      if ((desired_torque < lowest_rt_torque) || (desired_torque > highest_rt_torque)) {
        violation = 1;
      }

      // every RT_INTERVAL set the new limits
      uint32_t ts_elapsed = get_ts_elapsed(ts, ts_last);
      if (ts_elapsed > RT_INTERVAL) {
        cadillac_rt_torque_last = desired_torque;
        ts_last = ts;
      }
    }

    // no torque if controls is not allowed
    if (!controls_allowed && (desired_torque != 0)) {
      violation = 1;
    }

   // reset to 0 if either controls is not allowed or there's a violation
   if (violation || !controls_allowed) {
      cadillac_rt_torque_last = 0;
      ts_last = ts;
    }

    if (violation) {
      return false;
    }

  }
  return true;
}

static void cadillac_init(int16_t param) {
  cadillac_ign = 0;
}

static int cadillac_ign_hook() {
  uint32_t ts = TIM2->CNT;
  uint32_t ts_elapsed = get_ts_elapsed(ts, cadillac_ts_ign_last);
  if (ts_elapsed > CADILLAC_IGNITION_TIMEOUT) {
    cadillac_ign = 0;
  }
  return cadillac_ign;
}

// Placeholder file, actual safety is TODO.
const safety_hooks cadillac_hooks = {
  .init = cadillac_init,
  .rx = cadillac_rx_hook,
  .tx = cadillac_tx_hook,
  .tx_lin = alloutput_tx_lin_hook,
  .ignition = cadillac_ign_hook,
  .fwd = alloutput_fwd_hook,
};
