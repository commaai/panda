// board enforces
//   in-state
//      accel set/resume
//   out-state
//      cancel button
//      regen paddle
//      accel rising edge
//      brake rising edge
//      brake > 0mph

// OP-side EPS timing fix isn't preventing all EPS faults
// Workaround is in place until OP fix is completed
#define GM_EPS_TIMING_WORKAROUND

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
const CanMsg GM_TX_MSGS[] = {{384, 0, 4}, {1033, 0, 7}, {1034, 0, 7}, {715, 0, 8}, {789, 0, 5}, {880, 0, 6},  // pt bus
                             {161, 1, 7}, {774, 1, 8}, {776, 1, 7}, {784, 1, 2},   // obs bus
                             {789, 2, 5},  // ch bus
                             {0x104c006c, 3, 3}, {0x10400060, 3, 5}};  // gmlan

// TODO: do checksum and counter checks. Add correct timestep, 0.1s for now.
AddrCheckStruct gm_addr_checks[] = {
  {.msg = {{388, 0, 8, .expected_timestep = 100000U}, { 0 }, { 0 }}},
  {.msg = {{842, 0, 5, .expected_timestep = 100000U}, { 0 }, { 0 }}},
  {.msg = {{481, 0, 7, .expected_timestep = 100000U}, { 0 }, { 0 }}},
  {.msg = {{241, 0, 6, .expected_timestep = 100000U}, { 0 }, { 0 }}},
  {.msg = {{452, 0, 8, .expected_timestep = 100000U}, { 0 }, { 0 }}},
};
#define GM_RX_CHECK_LEN (sizeof(gm_addr_checks) / sizeof(gm_addr_checks[0]))
addr_checks gm_rx_checks = {gm_addr_checks, GM_RX_CHECK_LEN};

// Param Definitions
const uint16_t GM_PARAM_HARNESS_CAM = 1;
const uint16_t GM_PARAM_STOCK_LONG = 2;

// TODO: Update tests to include params
// TODO: If 1, check fwd. else check no fwd
// TODO: If 2, check 715 allowed else 715 kills / blocks controls 

enum {
  GM_BTN_UNPRESS = 1,
  GM_BTN_RESUME = 2,
  GM_BTN_SET = 3,
  GM_BTN_CANCEL = 6,
};

int gm_cam_bus = 2;

enum {GM_OBD2, GM_CAM} gm_harness = GM_OBD2;
bool gm_stock_long = false;

#ifdef GM_EPS_TIMING_WORKAROUND
  uint32_t gm_start_ts = 0;
  uint32_t gm_last_lkas_ts = 0;
#endif

static int gm_rx_hook(CANPacket_t *to_push) {

  bool valid = addr_safety_check(to_push, &gm_rx_checks, NULL, NULL, NULL);

  if (valid && (GET_BUS(to_push) == 0U)) {
    int addr = GET_ADDR(to_push);

    if (addr == 388) {
      int torque_driver_new = ((GET_BYTE(to_push, 6) & 0x7U) << 8) | GET_BYTE(to_push, 7);
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
      int button = (GET_BYTE(to_push, 5) & 0x70U) >> 4;

      // exit controls on cancel press
      if (button == GM_BTN_CANCEL) {
        controls_allowed = 0;
      }

      // enter controls on falling edge of set or resume
      bool set = (button == GM_BTN_UNPRESS) && (cruise_button_prev == GM_BTN_SET);
      bool res = (button == GM_BTN_UNPRESS) && (cruise_button_prev == GM_BTN_RESUME);
      if (set || res) {
        controls_allowed = 1;
      }

      cruise_button_prev = button;
    }

    // speed > 0
    if (addr == 241) {
      // Brake pedal's potentiometer returns near-zero reading
      // even when pedal is not pressed
      brake_pressed = GET_BYTE(to_push, 1) >= 10U;
    }

    if (addr == 452) {
      gas_pressed = GET_BYTE(to_push, 5) != 0U;
    }

    // exit controls on regen paddle
    if (addr == 189) {
      bool regen = GET_BYTE(to_push, 0) & 0x20U;
      if (regen) {
        controls_allowed = 0;
      }
    }

    // Check if ASCM or LKA camera are online
    // on powertrain bus.
    // 384 = ASCMLKASteeringCmd
    // 715 = ASCMGasRegenCmd
    // Allow ACC if using stock long
    generic_rx_checks(((addr == 384) || ((!gm_stock_long) && (addr == 715))));
  }
  return valid;
}

// all commands: gas/regen, friction brake and steering
// if controls_allowed and no pedals pressed
//     allow all commands up to limit
// else
//     block all commands that produce actuation

static int gm_tx_hook(CANPacket_t *to_send, bool longitudinal_allowed) {

  int tx = 1;
  int addr = GET_ADDR(to_send);

  if (!msg_allowed(to_send, GM_TX_MSGS, sizeof(GM_TX_MSGS)/sizeof(GM_TX_MSGS[0]))) {
    tx = 0;
  }

  // disallow actuator commands if gas or brake (with vehicle moving) are pressed
  // and the the latching controls_allowed flag is True
  int pedal_pressed = brake_pressed_prev && vehicle_moving;
  bool alt_exp_allow_gas = alternative_experience & ALT_EXP_DISABLE_DISENGAGE_ON_GAS;
  if (!alt_exp_allow_gas) {
    pedal_pressed = pedal_pressed || gas_pressed_prev;
  }
  bool current_controls_allowed = controls_allowed && !pedal_pressed;

  // BRAKE: safety check
  if (addr == 789) {
    int brake = ((GET_BYTE(to_send, 0) & 0xFU) << 8) + GET_BYTE(to_send, 1);
    brake = (0x1000 - brake) & 0xFFF;
    if (!current_controls_allowed || !longitudinal_allowed) {
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
    int desired_torque = ((GET_BYTE(to_send, 0) & 0x7U) << 8) + GET_BYTE(to_send, 1);
    uint32_t ts = microsecond_timer_get();
    bool violation = 0;
    desired_torque = to_signed(desired_torque, 11);

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

    #ifdef GM_EPS_TIMING_WORKAROUND
      // Drop LKAS frames that come in too fast. 20ms is the target while active, 
      // but "picky" PSCMs will fault under about 13ms
      // and OP misses the 20ms target quite frequently
      uint32_t ts2 = microsecond_timer_get();
      uint32_t ts_elapsed = get_ts_elapsed(ts2, gm_last_lkas_ts);
      if (ts_elapsed <= 13000) {
        tx = 0;
      }
      else {
        gm_last_lkas_ts = ts2;
      }
      // TODO BEFORE PR: A delay over 200ms while ACTIVE will also cause a fault
      // Simplest way to avoid this would be to inject an inactive frame
      // when starting to block (for any reason)
      // Note: Drooping LKAS frames is a protocol violation
      //       They should not really ever be dropped like this
      //       Instead, output should switch to inactive
      // Note 2: Per jyoung, the steering limits should practically never be tripped.
      //         Something is wrong with the limits - RT rate in particular is triggered
      //         Quite frequently
    #endif
  }


  // GAS/REGEN: safety check
  if (addr == 715) {
    int gas_regen = ((GET_BYTE(to_send, 2) & 0x7FU) << 5) + ((GET_BYTE(to_send, 3) & 0xF8U) >> 3);
    // Disabled message is !engaged with gas
    // value that corresponds to max regen.
    if (!current_controls_allowed || !longitudinal_allowed) {
      // Stock ECU sends max regen when not enabled
      if (gas_regen != GM_MAX_REGEN) {
        tx = 0;
      }
    }
    // Need to allow apply bit in pre-enabled and overriding states
    if (!controls_allowed) {
      bool apply = GET_BIT(to_send, 0U) != 0U;
      if (apply) {
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

static int gm_fwd_hook(int bus_num, CANPacket_t *to_fwd) {
  int bus_fwd = -1;
  if (gm_harness == GM_CAM) {
    if (bus_num == 0) {
      // Fwd PT to cam
      bus_fwd = gm_cam_bus;
    }
    else if (bus_num == gm_cam_bus) {
      // Fwd cam to PT
      // block stock lkas messages and stock acc messages (if OP is doing ACC)
      int addr = GET_ADDR(to_fwd);
      bool is_lkas_msg = (addr == 384);
      // Keepalives: (addr == 1033) || (addr == 1034)
      // TODO: carcontroller.py omit keepalives using cam harness
      bool is_acc_msg = ((addr == 715) || (addr == 880) || (addr == 789) || (addr == 1033) || (addr == 1034));
      bool block_fwd = (is_lkas_msg || (is_acc_msg && !gm_stock_long));
      if (!block_fwd) {
        bus_fwd = 0;
      }
    }
  }
  return bus_fwd;
}


static const addr_checks* gm_init(uint16_t param) {
  gm_stock_long = GET_FLAG(param, GM_PARAM_STOCK_LONG);
  gm_harness = (GET_FLAG(param, GM_PARAM_HARNESS_CAM) ? (GM_CAM) : (GM_OBD2));
  #ifdef GM_EPS_TIMING_WORKAROUND
    gm_start_ts = microsecond_timer_get();
  #endif
  return &gm_rx_checks;
}

const safety_hooks gm_hooks = {
  .init = gm_init,
  .rx = gm_rx_hook,
  .tx = gm_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = gm_fwd_hook,
};
