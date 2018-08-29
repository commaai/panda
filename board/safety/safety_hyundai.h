const int HYUNDAI_MAX_STEER = 250;
const int HYUNDAI_MAX_RT_DELTA = 128;          // max delta torque allowed for real time checks
const int32_t HYUNDAI_RT_INTERVAL = 250000;    // 250ms between real time checks
const int HYUNDAI_MAX_RATE_UP = 4;
const int HYUNDAI_MAX_RATE_DOWN = 10;
const int HYUNDAI_DRIVER_TORQUE_ALLOWANCE = 50;
const int HYUNDAI_DRIVER_TORQUE_FACTOR = 4;

int hyundai_brake_prev = 0;
int hyundai_gas_prev = 0;
int hyundai_speed = 0;
int hyundai_camera_detected = 0;
int hyundai_rt_torque_last = 0;
int hyundai_desired_torque_last = 0;
int hyundai_cruise_engaged_last = 0;
uint32_t hyundai_ts_last = 0;
struct sample_t hyundai_torque_driver;         // last few driver torques measured

static void hyundai_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {
  int bus_number = (to_push->RDTR >> 4) & 0xFF;
  uint32_t addr;
  if (to_push->RIR & 4) {
    // Extended
    // Not looked at, but have to be separated
    // to avoid address collision
    addr = to_push->RIR >> 3;
  } else {
    // Normal
    addr = to_push->RIR >> 21;
  }

  if (addr == 897) {
    int torque_driver_new = ((to_push->RDHR >> 11) & 0x7ff) - 2048;
    // update array of samples
    update_sample(&hyundai_torque_driver, torque_driver_new);
  }

  // check if stock camera ECU is still online
  if (bus_number == 0 && addr == 832) {
    hyundai_camera_detected = 1;
    controls_allowed = 0;
  }

  // enter controls on rising edge of ACC, exit controls on ACC off
  if ((to_push->RIR>>21) == 1057) {
    // 2 bits: 13-14
    int cruise_engaged = (to_push->RDLR >> 13) & 0x3;
    if (cruise_engaged && !hyundai_cruise_engaged_last) {
      controls_allowed = 1;
    } else if (!cruise_engaged) {
      controls_allowed = 0;
    }
    hyundai_cruise_engaged_last = cruise_engaged;
  }
}

static int hyundai_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {

  // There can be only one! (camera)
  if (hyundai_camera_detected) {
    return 0;
  }

  // disallow actuator commands if gas or brake (with vehicle moving) are pressed
  // and the the latching controls_allowed flag is True
  int pedal_pressed = hyundai_gas_prev || (hyundai_brake_prev && hyundai_speed);
  int current_controls_allowed = controls_allowed && !pedal_pressed;

  uint32_t addr;
  if (to_send->RIR & 4) {
    // Extended
    addr = to_send->RIR >> 3;
  } else {
    // Normal
    addr = to_send->RIR >> 21;
  }

  // LKA STEER: safety check
  if (addr == 832) {
    int rdlr = to_send->RDLR;
    int desired_torque = ((rdlr >> 16) & 0x7ff) - 1024;
    uint32_t ts = TIM2->CNT;
    int violation = 0;

    if (current_controls_allowed) {

      // *** global torque limit check ***
      violation |= max_limit_check(desired_torque, HYUNDAI_MAX_STEER, -HYUNDAI_MAX_STEER);

      // *** torque rate limit check ***
      violation |= driver_limit_check(desired_torque, hyundai_desired_torque_last, &hyundai_torque_driver,
        HYUNDAI_MAX_STEER, HYUNDAI_MAX_RATE_UP, HYUNDAI_MAX_RATE_DOWN,
        HYUNDAI_DRIVER_TORQUE_ALLOWANCE, HYUNDAI_DRIVER_TORQUE_FACTOR);

      // used next time
      hyundai_desired_torque_last = desired_torque;

      // *** torque real time rate limit check ***
      violation |= rt_rate_limit_check(desired_torque, hyundai_rt_torque_last, HYUNDAI_MAX_RT_DELTA);

      // every RT_INTERVAL set the new limits
      uint32_t ts_elapsed = get_ts_elapsed(ts, hyundai_ts_last);
      if (ts_elapsed > HYUNDAI_RT_INTERVAL) {
        hyundai_rt_torque_last = desired_torque;
        hyundai_ts_last = ts;
      }
    }

    // no torque if controls is not allowed
    if (!current_controls_allowed && (desired_torque != 0)) {
      violation = 1;
    }

    // reset to 0 if either controls is not allowed or there's a violation
    if (violation || !current_controls_allowed) {
      hyundai_desired_torque_last = 0;
      hyundai_rt_torque_last = 0;
      hyundai_ts_last = ts;
    }

    if (violation) {
      return false;
    }
  }

  // 1 allows the message through
  return true;
}

const safety_hooks hyundai_hooks = {
  .init = nooutput_init,
  .rx = hyundai_rx_hook,
  .tx = hyundai_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .ignition = default_ign_hook,
  .fwd = nooutput_fwd_hook,
};
