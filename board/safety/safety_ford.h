// board enforces
//   in-state
//      accel set/resume
//   out-state
//      cancel button
//      accel rising edge
//      brake rising edge
//      brake > 0mph

int brake_prev = 0;
int gas_prev = 0;
int ego_speed = 0;

static void ford_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {

  // sample speed
  if ((to_push->RIR>>21) == 0x158) {
    // first 2 bytes
    ego_speed = to_push->RDLR & 0xFFFF;
  }

  // state machine to enter and exit controls
  // 0x1A6 for the ILX, 0x296 for the Civic Touring
  if ((to_push->RIR>>21) == 0x1A6) {
    int buttons = (to_push->RDLR & 0xE0) >> 5;
    if (buttons == 4 || buttons == 3) {
      controls_allowed = 1;
    } else if (buttons == 2) {
      controls_allowed = 0;
    }
  }

  // exit controls on rising edge of brake press or on brake press when
  // speed > 0
  if ((to_push->RIR>>21) == 0x17D) {
    int brake = to_push->RDLR & 0xFF;
    if (brake && (!(brake_prev) || ego_speed)) {
      controls_allowed = 0;
    }
    brake_prev = brake;
  }

  // exit controls on rising edge of gas press
  if ((to_push->RIR>>21) == 0x17C) {
    int gas = to_push->RDLR & 0xFF;
    if (gas && !(gas_prev)) {
      controls_allowed = 0;
    }
    gas_prev = gas;
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
  int pedal_pressed = gas_prev || (brake_prev && ego_speed);
  int current_controls_allowed = controls_allowed && !(pedal_pressed);

  // STEER: safety check
  if ((to_send->RIR>>21) == 0xE4 || (to_send->RIR>>21) == 0x194) {
    if (current_controls_allowed) {
      // all messages are fine here
    } else {
      if ((to_send->RDLR & 0xFFFF0000) != to_send->RDLR) return 0;
    }
  }

  // 1 allows the message through
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
