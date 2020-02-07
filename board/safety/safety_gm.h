// board enforces
//   in-state
//      accel set/resume
//   out-state
//      cancel button
//      regen paddle
//      accel rising edge
//      brake rising edge
//      brake > 0mph

typedef struct {
  uint32_t current_ts;
  CAN_FIFOMailBox_TypeDef current_frame;
  uint32_t stock_ts;
  CAN_FIFOMailBox_TypeDef stock_frame;
  uint32_t op_ts;
  CAN_FIFOMailBox_TypeDef op_frame;
  uint32_t rolling_counter;
} gm_dual_buffer;

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
const AddrBus GM_TX_MSGS[] = {{384, 0}, {1033, 0}, {1034, 0}, {715, 0}, {880, 0},  // pt bus
                              {161, 1}, {774, 1}, {776, 1}, {784, 1},   // obs bus
                              {789, 2},  // ch bus
                              {0x104c006c, 3}, {0x10400060, 3}};  // gmlan

// TODO: do checksum and counter checks. Add correct timestep, 0.1s for now.
AddrCheckStruct gm_rx_checks[] = {
  {.addr = {388}, .bus = 0, .expected_timestep = 100000U},
  {.addr = {842}, .bus = 0, .expected_timestep = 100000U},
  {.addr = {481}, .bus = 0, .expected_timestep = 100000U},
  {.addr = {241}, .bus = 0, .expected_timestep = 100000U},
  {.addr = {417}, .bus = 0, .expected_timestep = 100000U},
};
const int GM_RX_CHECK_LEN = sizeof(gm_rx_checks) / sizeof(gm_rx_checks[0]);

int gm_brake_prev = 0;
int gm_gas_prev = 0;
bool gm_moving = false;
int gm_rt_torque_last = 0;
int gm_desired_torque_last = 0;
uint32_t gm_ts_last = 0;
struct sample_t gm_torque_driver;         // last few driver torques measured

static void gm_init_lkas_pump(void);

volatile gm_dual_buffer gm_lkas_buffer;

volatile bool gm_ffc_detected = false;

//Copy the stock or OP buffer into the currrent buffer
static void gm_apply_buffer(volatile gm_dual_buffer *buffer, bool stock) {
  if (stock) {
    buffer->current_frame.RIR = buffer->stock_frame.RIR | 1;
    buffer->current_frame.RDTR = buffer->stock_frame.RDTR;
    buffer->current_frame.RDLR = buffer->stock_frame.RDLR;
    buffer->current_frame.RDHR = buffer->stock_frame.RDHR;
    buffer->current_ts = buffer->stock_ts;
  } else {
    buffer->current_frame.RIR = buffer->op_frame.RIR;
    buffer->current_frame.RDTR = buffer->op_frame.RDTR;
    buffer->current_frame.RDLR = buffer->op_frame.RDLR;
    buffer->current_frame.RDHR = buffer->op_frame.RDHR;
    buffer->current_ts = buffer->op_ts;
  }
}

//Populate the stock lkas - called by fwd hook
static void gm_set_stock_lkas(CAN_FIFOMailBox_TypeDef *to_send) {
  gm_lkas_buffer.stock_frame.RIR = to_send->RIR;
  gm_lkas_buffer.stock_frame.RDTR = to_send->RDTR;
  gm_lkas_buffer.stock_frame.RDLR = to_send->RDLR;
  gm_lkas_buffer.stock_frame.RDHR = to_send->RDHR;
  gm_lkas_buffer.stock_ts = TIM2->CNT;
}

//Populate the OP lkas - called by tx hook
static void gm_set_op_lkas(CAN_FIFOMailBox_TypeDef *to_send) {
  gm_lkas_buffer.op_frame.RIR = to_send->RIR;
  gm_lkas_buffer.op_frame.RDTR = to_send->RDTR;
  gm_lkas_buffer.op_frame.RDLR = to_send->RDLR;
  gm_lkas_buffer.op_frame.RDHR = to_send->RDHR;
  gm_lkas_buffer.op_ts = TIM2->CNT;
}

static int gm_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {

  bool valid = addr_safety_check(to_push, gm_rx_checks, GM_RX_CHECK_LEN,
                                 NULL, NULL, NULL);

  if (valid) {
    int bus = GET_BUS(to_push);
    int addr = GET_ADDR(to_push);

    if (addr == 388) {
      int torque_driver_new = ((GET_BYTE(to_push, 6) & 0x7) << 8) | GET_BYTE(to_push, 7);
      torque_driver_new = to_signed(torque_driver_new, 11);
      // update array of samples
      update_sample(&gm_torque_driver, torque_driver_new);
    }

    // sample speed, really only care if car is moving or not
    // rear left wheel speed
    if (addr == 842) {
      gm_moving = GET_BYTE(to_push, 0) | GET_BYTE(to_push, 1);
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

    // exit controls on rising edge of brake press or on brake press when
    // speed > 0
    if (addr == 241) {
      int brake = GET_BYTE(to_push, 1);
      // Brake pedal's potentiometer returns near-zero reading
      // even when pedal is not pressed
      if (brake < 10) {
        brake = 0;
      }
      if (brake && (!gm_brake_prev || gm_moving)) {
         controls_allowed = 0;
      }
      gm_brake_prev = brake;
    }

    // exit controls on rising edge of gas press
    if (addr == 417) {
      int gas = GET_BYTE(to_push, 6);
      if (gas && !gm_gas_prev) {
        controls_allowed = 0;
      }
      gm_gas_prev = gas;
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
    if ((safety_mode_cnt > RELAY_TRNS_TIMEOUT) && (bus == 0) && ((addr == 384) || (addr == 715))) {
      relay_malfunction = true;
    }
  }
  return valid;
}

// all commands: gas/regen, friction brake and steering
// if controls_allowed and no pedals pressed
//     allow all commands up to limit
// else
//     block all commands that produce actuation

static int gm_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {

  int tx = 1;
  int addr = GET_ADDR(to_send);
  int bus = GET_BUS(to_send);

  if (!msg_allowed(addr, bus, GM_TX_MSGS, sizeof(GM_TX_MSGS)/sizeof(GM_TX_MSGS[0]))) {
    tx = 0;
  }

  if (relay_malfunction) {
    tx = 0;
  }

  // disallow actuator commands if gas or brake (with vehicle moving) are pressed
  // and the the latching controls_allowed flag is True
  int pedal_pressed = gm_gas_prev || (gm_brake_prev && gm_moving);
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
    //precalculated inactive zero values to be sent when there is a violation or inactivation
    uint32_t vals[4];
    vals[0] = 0x00000000U;
    vals[1] = 0x10000fffU;
    vals[2] = 0x20000ffeU;
    vals[3] = 0x30000ffdU;

    int rolling_counter = GET_BYTE(to_send, 0) >> 4;
    int desired_torque = ((GET_BYTE(to_send, 0) & 0x7U) << 8) + GET_BYTE(to_send, 1);
    uint32_t ts = TIM2->CNT;
    bool violation = 0;
    desired_torque = to_signed(desired_torque, 11);

    if (current_controls_allowed) {

      // *** global torque limit check ***
      violation |= max_limit_check(desired_torque, GM_MAX_STEER, -GM_MAX_STEER);

      // *** torque rate limit check ***
      violation |= driver_limit_check(desired_torque, gm_desired_torque_last, &gm_torque_driver,
        GM_MAX_STEER, GM_MAX_RATE_UP, GM_MAX_RATE_DOWN,
        GM_DRIVER_TORQUE_ALLOWANCE, GM_DRIVER_TORQUE_FACTOR);

      // used next time
      gm_desired_torque_last = desired_torque;

      // *** torque real time rate limit check ***
      violation |= rt_rate_limit_check(desired_torque, gm_rt_torque_last, GM_MAX_RT_DELTA);

      // every RT_INTERVAL set the new limits
      uint32_t ts_elapsed = get_ts_elapsed(ts, gm_ts_last);
      if (ts_elapsed > GM_RT_INTERVAL) {
        gm_rt_torque_last = desired_torque;
        gm_ts_last = ts;
      }
    }

    // no torque if controls is not allowed
    if (!current_controls_allowed && (desired_torque != 0)) {
      violation = 1;
    }

    // reset to 0 if either controls is not allowed or there's a violation
    if (violation || !current_controls_allowed) {
      gm_desired_torque_last = 0;
      gm_rt_torque_last = 0;
      gm_ts_last = ts;
    }

    if (violation) {
      //Replace payload with appropriate zero value for expected rolling counter
      to_send->RDLR = vals[rolling_counter];
    }
    tx = 0; //we never tx LKAS - it is buffered
    gm_set_op_lkas(to_send); //apply the OP LKAS to the buffer
    gm_init_lkas_pump(); //ensure the message pump is active
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
  int bus_fwd = -1;
  if (bus_num == 0) {
    if (gm_ffc_detected) {
      //only perform forwarding if we have seen LKAS messages on CAN2
      bus_fwd = 1;  // Camera is on CAN2
    }
  }
  if (bus_num == 1) {
    int addr = GET_ADDR(to_fwd);
    if (addr != 384) {
      //only perform forwarding if we have seen LKAS messages on CAN2
      if (gm_ffc_detected) {
        return 0;
      }
    }
    gm_set_stock_lkas(to_fwd);
    gm_ffc_detected = true;
    gm_init_lkas_pump();
  }

  // fallback to do not forward
  return bus_fwd;
}


static CAN_FIFOMailBox_TypeDef * gm_pump_hook(void) {
  volatile int pedal_pressed = (volatile int)gm_gas_prev || ((volatile int)gm_brake_prev && (volatile int)gm_moving);
  volatile bool current_controls_allowed = (volatile bool)controls_allowed && !(volatile int)pedal_pressed;

  if (!gm_ffc_detected) {
    //If we haven't seen lkas messages from CAN2, there is no passthrough, just use OP
    if (gm_lkas_buffer.op_ts == 0) return NULL;
    //puts("using OP lkas\n");
    gm_apply_buffer(&gm_lkas_buffer, false);
    //In OP only mode we need to send zero if controls are not allowed
    if (!current_controls_allowed) {
      gm_lkas_buffer.current_frame.RDLR = 0U;
      gm_lkas_buffer.current_frame.RDHR = 0U;
    }
  }
  else 
  {
    if (!current_controls_allowed) {
      if (gm_lkas_buffer.stock_ts == 0) return NULL;
      //puts("using stock lkas\n");
      gm_apply_buffer(&gm_lkas_buffer, true);
    } else {
      if (gm_lkas_buffer.op_ts == 0) return NULL;
      //puts("using OP lkas\n");
      gm_apply_buffer(&gm_lkas_buffer, false);
    }
  }

  // If the timestamp of the last message is more than 40ms old, fallback to zero
  // If it is more than 1 second, disable message pump
  uint32_t ts = TIM2->CNT;
  uint32_t ts_elapsed = get_ts_elapsed(ts, gm_lkas_buffer.current_ts);



  if (ts_elapsed > 1000000u) {
    puts("Disabling message pump due to stale buffer\n");
    disable_message_pump();
  } else if (ts_elapsed > 40000u) {
    puts("Zeroing frame due to stale buffer: ");
    puth(ts_elapsed);
    puts("\n");
    gm_lkas_buffer.current_frame.RDLR = 0U;
    gm_lkas_buffer.current_frame.RDHR = 0U;
  }

  gm_lkas_buffer.rolling_counter = (gm_lkas_buffer.rolling_counter + 1) % 4;

  //update the rolling counter
  gm_lkas_buffer.current_frame.RDLR = (0xFFFFFFCF & gm_lkas_buffer.current_frame.RDLR) | (gm_lkas_buffer.rolling_counter << 4);

  // Recalculate checksum - Thanks Andrew C

  // Replacement rolling counter 
  uint32_t newidx = gm_lkas_buffer.rolling_counter;
  
  // Pull out LKA Steering CMD data and swap endianness (not including rolling counter)
  uint32_t dataswap = ((gm_lkas_buffer.current_frame.RDLR << 8) & 0x0F00U) | ((gm_lkas_buffer.current_frame.RDLR >> 8) &0xFFU);

  // Compute Checksum
  uint32_t checksum = (0x1000 - dataswap - newidx) & 0x0fff;
  
  //Swap endianness of checksum back to what GM expects
  uint32_t checksumswap = (checksum >> 8) | ((checksum << 8) & 0xFF00U);
  
  // Merge the rewritten checksum back into the BxCAN frame RDLR
  gm_lkas_buffer.current_frame.RDLR &= 0x0000FFFF;
  gm_lkas_buffer.current_frame.RDLR |= (checksumswap << 16);

  return (CAN_FIFOMailBox_TypeDef*)&gm_lkas_buffer.current_frame;
}

static void gm_init_lkas_pump() {
  if (message_pump_active) return;
  puts("Starting message pump\n");
  enable_message_pump(15, gm_pump_hook);
}


const safety_hooks gm_hooks = {
  .init = nooutput_init,
  .rx = gm_rx_hook,
  .tx = gm_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = gm_fwd_hook,
  .addr_check = gm_rx_checks,
  .addr_check_len = sizeof(gm_rx_checks) / sizeof(gm_rx_checks[0]),
};
