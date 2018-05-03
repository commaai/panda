// board enforces
//   in-state
//      accel set/resume
//   out-state
//      cancel button
//      accel rising edge
//      brake rising edge
//      brake > 0mph

int brake_prev_ford = 0;
int gas_prev_ford = 0;
int ego_speed_ford = 0;

static void ford_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {

  // sample speed
  int wheel_bits = 0xFCFF;
  if ((to_push->RIR>>21) == 0x217) {
    // any of the 14 bits wheel speeds > 0?
    ego_speed_ford = (to_push->RDLR & wheel_bits) |
                     (to_push->RDHR & wheel_bits) |
                     ((to_push->RDLR >> 16) & wheel_bits) |
                     ((to_push->RDHR >> 16) & wheel_bits);
  }

  // state machine to enter and exit controls
  // 0x1A6 for the ILX, 0x296 for the Civic Touring
  if ((to_push->RIR>>21) == 0x83) {
    int cancel = ((to_push->RDLR >> 8) & 0x1);
    int set_or_resume = (to_push->RDLR >> 28) & 0x3;
    if (cancel) {
      controls_allowed = 0;
    } else if (set_or_resume) {
      controls_allowed = 1;
    }
  }

  // exit controls on rising edge of brake press or on brake press when
  // speed > 0
  if ((to_push->RIR>>21) == 0x165) {
    int brake = to_push->RDLR & 0x20;
    if (brake && (!(brake_prev_ford) || ego_speed_ford)) {
      controls_allowed = 0;
    }
    brake_prev_ford = brake;
  }

  // exit controls on rising edge of gas press
  if ((to_push->RIR>>21) == 0x204) {
    int gas = to_push->RDLR & 0xFF03;
    if (gas && !(gas_prev_ford)) {
      controls_allowed = 0;
    }
    gas_prev_ford = gas;
  }
}

// all commands: just steering
// if controls_allowed and no pedals pressed
//     allow all commands up to limit
// else
//     block all commands that produce actuation

static int ford_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {

  // disallow actuator commands if gas or brake (with vehicle moving) are pressed
  // and the the latching controls_allowed flag is True
  int pedal_pressed = gas_prev_ford || (brake_prev_ford && ego_speed_ford);
  int current_controls_allowed = controls_allowed && !(pedal_pressed);

  // STEER: safety check
  if ((to_send->RIR>>21) == 0x3CA) {
    if (current_controls_allowed) {
      // all messages are fine here
    } else {
      // bits 7-4 need to be 0xF to disallow lkas commands
      if (((to_send->RDLR >> 4) & 0xF) != 0xF) return 0;
    }
  }

  // FORCE CANCEL: safety check only relevant when spamming the cancel button
  // ensuring that set and resume aren't sent
  if ((to_send->RIR>>21) == 0x83) {
    if ((to_send->RDLR >> 28) & 0x3) return 0;
  }

  // 1 allows the message through
  return true;
}

static int ford_tx_lin_hook(int lin_num, uint8_t *data, int len) {
  // TODO: add safety if using LIN
  return true;
}

static void ford_init(int16_t param) {
  controls_allowed = 0;
}

static int ford_fwd_hook(int bus_num, CAN_FIFOMailBox_TypeDef *to_fwd) {
  return -1;
}

static int ford_ign_hook() {
  return -1;
}

const safety_hooks ford_hooks = {
  .init = ford_init,
  .rx = ford_rx_hook,
  .tx = ford_tx_hook,
  .tx_lin = ford_tx_lin_hook,
  .ignition = ford_ign_hook,
  .fwd = ford_fwd_hook,
};
