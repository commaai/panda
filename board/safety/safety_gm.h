// board enforces
//   in-state
//      accel set/resume
//   out-state
//      cancel button
//      regen paddle
//      accel rising edge
//      brake rising edge
//      brake > 0mph

const int GM_MAX_STEER = 300;
const int GM_MAX_RT_DELTA = 128;          // max delta torque allowed for real time checks
const uint32_t GM_RT_INTERVAL = 250000;    // 250ms between real time checks
const int GM_MAX_RATE_UP = 7;
const int GM_MAX_RATE_DOWN = 17;
const int GM_DRIVER_TORQUE_ALLOWANCE = 50;
const int GM_DRIVER_TORQUE_FACTOR = 4;
const int GM_MAX_GAS = 3072;
const int GM_MAX_REGEN = 1404;
const int GM_MAX_BRAKE = 350;
const CanMsg GM_TX_MSGS[] = {{384, 0, 4}, {1033, 0, 7}, {1034, 0, 7}, {715, 0, 8}, {880, 0, 6}, {512, 0, 6}  // pt bus
                             {161, 1, 7}, {774, 1, 8}, {776, 1, 7}, {784, 1, 2},   // obs bus
                             {789, 2, 5},  // ch bus
                             {0x104c006c, 3, 3}, {0x10400060, 3, 5}};  // gmlan

// TODO: do checksum and counter checks. Add correct timestep, 0.1s for now.
AddrCheckStruct gm_rx_checks[] = {
  {.msg = {{388, 0, 8, .expected_timestep = 100000U}}},
  {.msg = {{842, 0, 5, .expected_timestep = 100000U}}},
  {.msg = {{481, 0, 7, .expected_timestep = 100000U}}},
  {.msg = {{241, 0, 6, .expected_timestep = 100000U}}},
  {.msg = {{417, 0, 7, .expected_timestep = 100000U}}},
};
const int GM_RX_CHECK_LEN = sizeof(gm_rx_checks) / sizeof(gm_rx_checks[0]);

//tracking the last published rolling counter
//uint32_t gm_rc_aeb = 0;
uint32_t gm_rc_lkas = 5;

bool gm_relay_open = false;
bool gm_relay_desired_open = true;
bool gm_ffc_detected = false; //only true when we have seen the ffc on camera bus
int gm_camera_bus = -1;

//wait for an lkas message, cut relay, save rc, wait for correct RC
static bool gm_handle_relay(CAN_FIFOMailBox_TypeDef *to_push) {
  if (!board_has_relay()) return true;
  if (gm_relay_open == gm_relay_desired_open) return true;

  if (gm_relay_desired_open) {
    //closed to open, we only care about PT bus
    if (GET_BUS(to_push) != 0) return false;
    int addr = GET_ADDR(to_push);
    if (addr != 384) return false;
    gm_rc_lkas = GET_BYTE(to_send, 0) >> 4;
    set_intercept_relay(true);
    //TODO: this assumes relay change is near-instant. If it lags, some values could sneak through...
    heartbeat_counter = 0U;
    return true;
  }
  //TODO: Open -> Closed


}



static int gm_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {
  if (!gm_handle_relay(to_push)) return 0;
  bool valid = addr_safety_check(to_push, gm_rx_checks, GM_RX_CHECK_LEN,
                                 NULL, NULL, NULL);

  if (valid && (GET_BUS(to_push) == 0)) {
    int addr = GET_ADDR(to_push);

    if (addr == 388) {
      int torque_driver_new = ((GET_BYTE(to_push, 6) & 0x7) << 8) | GET_BYTE(to_push, 7);
      torque_driver_new = to_signed(torque_driver_new, 11);
      // update array of samples
      update_sample(&torque_driver, torque_driver_new);
    }

    // sample speed, really only care if car is moving or not
    // rear left wheel speed
    if (addr == 842) {
      vehicle_moving = GET_BYTE(to_push, 0) | GET_BYTE(to_push, 1);
    }

    // ACC steering wheel buttons
    if (addr == 481) {
      int button = (GET_BYTE(to_push, 5) & 0x70) >> 4;
      switch (button) {
        case 2:  // resume
        case 3:  // set
          controls_allowed = 1;
          break;
        case 6:  // cancel
          controls_allowed = 0;
          break;
        default:
          break;  // any other button is irrelevant
      }
    }

    // speed > 0
    if (addr == 241) {
      // Brake pedal's potentiometer returns near-zero reading
      // even when pedal is not pressed
      brake_pressed = GET_BYTE(to_push, 1) >= 10;
    }

    if (addr == 417) {
      gas_pressed = GET_BYTE(to_push, 6) != 0;
    }

    // exit controls on regen paddle
    if (addr == 189) {
      bool regen = GET_BYTE(to_push, 0) & 0x20;
      if (regen) {
        controls_allowed = 0;
      }
    }

    // Check if ASCM or LKA camera are online
    // on powertrain bus.
    // 384 = ASCMLKASteeringCmd
    // 715 = ASCMGasRegenCmd
    generic_rx_checks(((addr == 384) || (addr == 715)));
  }
  return valid;
}

// all commands: gas/regen, friction brake and steering
// if controls_allowed and no pedals pressed
//     allow all commands up to limit
// else
//     block all commands that produce actuation

static int gm_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {
  if (!gm_handle_relay(to_send)) return 0; // We do nothing till relay is opened
  int tx = 1;
  int addr = GET_ADDR(to_send);

  if (!msg_allowed(to_send, GM_TX_MSGS, sizeof(GM_TX_MSGS)/sizeof(GM_TX_MSGS[0]))) {
    tx = 0;
  }

  if (relay_malfunction) {
    tx = 0;
  }

  // disallow actuator commands if gas or brake (with vehicle moving) are pressed
  // and the the latching controls_allowed flag is True
  int pedal_pressed = brake_pressed_prev && vehicle_moving;
  bool unsafe_allow_gas = unsafe_mode & UNSAFE_DISABLE_DISENGAGE_ON_GAS;
  if (!unsafe_allow_gas) {
    pedal_pressed = pedal_pressed || gas_pressed_prev;
  }
  bool current_controls_allowed = controls_allowed && !pedal_pressed;

  // BRAKE: safety check
  if (addr == 789) {
    int brake = ((GET_BYTE(to_send, 0) & 0xFU) << 8) + GET_BYTE(to_send, 1);
    brake = (0x1000 - brake) & 0xFFF;
    if (!current_controls_allowed) {
      if (brake != 0) {
        tx = 0;
      }
    }
    if (brake > GM_MAX_BRAKE) {
      tx = 0;
    }
  }

  // LKA STEER: safety check
  if (addr == 384) {
    int rolling_counter = GET_BYTE(to_send, 0) >> 4;
    int desired_torque = ((GET_BYTE(to_send, 0) & 0x7U) << 8) + GET_BYTE(to_send, 1);
    uint32_t ts = TIM2->CNT;
    bool violation = 0;
    desired_torque = to_signed(desired_torque, 11);

    // This esures that if a message is skipped, we wait until it comes back around to the correct value
    // LKAS messages must always have a rolling counter in-order with no gaps.
    // This will drop up to 3 messages - up to 60ms lag. Totally acceptable.
    if (rolling_counter != (gm_rc_lkas + 1) % 4) violation = true;

    if (current_controls_allowed) {

      // *** global torque limit check ***
      violation |= max_limit_check(desired_torque, GM_MAX_STEER, -GM_MAX_STEER);

      // *** torque rate limit check ***
      violation |= driver_limit_check(desired_torque, desired_torque_last, &torque_driver,
        GM_MAX_STEER, GM_MAX_RATE_UP, GM_MAX_RATE_DOWN,
        GM_DRIVER_TORQUE_ALLOWANCE, GM_DRIVER_TORQUE_FACTOR);

      // used next time
      desired_torque_last = desired_torque;

      // *** torque real time rate limit check ***
      violation |= rt_rate_limit_check(desired_torque, rt_torque_last, GM_MAX_RT_DELTA);

      // every RT_INTERVAL set the new limits
      uint32_t ts_elapsed = get_ts_elapsed(ts, ts_last);
      if (ts_elapsed > GM_RT_INTERVAL) {
        rt_torque_last = desired_torque;
        ts_last = ts;
      }
      //TODO: may require tuning. TODO: hopefully the timing can be caught here. If not, the lag is downstream TODO: debug output
      //We need to drop lkas frame when comes in too fast.
      // (we will then skip up to 4 frames)
      if (ts_elapsed < 20000) violation = true;
    }

    // no torque if controls is not allowed
    if (!current_controls_allowed && (desired_torque != 0)) {
      violation = 1;
    }

    // reset to 0 if either controls is not allowed or there's a violation
    if (violation || !current_controls_allowed) {
      desired_torque_last = 0;
      rt_torque_last = 0;
      ts_last = ts;
    }

    if (violation) {
      tx = 0;
    }
    else {
      //If we are transmitting the lkas, update the rc
      gm_rc_lkas = rolling_counter;
    }
  }

  // GAS/REGEN: safety check
  if (addr == 715) {
    int gas_regen = ((GET_BYTE(to_send, 2) & 0x7FU) << 5) + ((GET_BYTE(to_send, 3) & 0xF8U) >> 3);
    // Disabled message is !engaged with gas
    // value that corresponds to max regen.
    if (!current_controls_allowed) {
      bool apply = GET_BYTE(to_send, 0) & 1U;
      if (apply || (gas_regen != GM_MAX_REGEN)) {
        tx = 0;
      }
    }
    if (gas_regen > GM_MAX_GAS) {
      tx = 0;
    }
  }

  // 1 allows the message through
  return tx;
}

static int gm_fwd_hook(int bus_num, CAN_FIFOMailBox_TypeDef *to_fwd) {
  if (!gm_handle_relay(to_fwd)) return 0; //for now, when relay is closed we don't want to do anything
  int bus_fwd = -1;
  if (bus_num == 0) {
    if (gm_ffc_detected) {
      //only perform forwarding if we have seen LKAS messages on CAN2
      bus_fwd = gm_camera_bus;  // Camera is on CAN2
    }
  }
  if (bus_num == gm_camera_bus) {
    int addr = GET_ADDR(to_fwd);
    if (addr != 384) {
      //only perform forwarding if we have seen LKAS messages on CAN2
      if (gm_ffc_detected) {
        return 0;
      }
    }
    gm_ffc_detected = true;
  }
  // fallback to do not forward
  return bus_fwd;
}

static void gm_init_hook(int16_t param) {
  if (board_has_relay()) {
    gm_camera_bus = 2;
  }
  else {
    gm_camera_bus = 1;
  }
}


const safety_hooks gm_hooks = {
  .init = gm_init_hook,
  .rx = gm_rx_hook,
  .tx = gm_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = gm_fwd_hook,
  .addr_check = gm_rx_checks,
  .addr_check_len = sizeof(gm_rx_checks) / sizeof(gm_rx_checks[0]),
};
