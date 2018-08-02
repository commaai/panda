// board enforces
//   in-state
//      ACC is active (green)
//   out-state
//      ACC cancel button
//      brake pressed
//      accel pressed
//      stock LKAS ECU is online

// chrysler_: namespacing
int chrysler_speed = 0;

// silence everything if stock ECUs are still online
int chrysler_lkas_detected = 0;

int chrysler_ignition_started = 0;

static void chrysler_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {
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

  // Gear selector used for determining ignition.
  if (addr == 0x2ea && bus_number == 0) {
    int gear = to_push->RDLR & 0x7;
    chrysler_ignition_started = gear > 0; //Park = 0. If out of park, we're "on."
  }

  if (addr == 0x144 && bus_number == 0) {
    chrysler_speed = ((to_push->RDLR & 0xFF000000) >> 16) | (to_push->RDHR & 0xFF);
  }

  // check if stock LKAS ECU is still online
  if (addr == 0x292 && bus_number == 0) {
    chrysler_lkas_detected = 1;
    controls_allowed = 0;
  }

  // ["ACC_2"]['ACC_STATUS_2'] == 7 for active (green) Adaptive Cruise Control
  if (addr == 0x1f4 && bus_number == 0) {
    if (((to_push->RDLR & 0x380000) >> 19) == 7) {
      controls_allowed = 1;
    }
  }

  // exit controls on brake press by human
  if (addr == 0x140) {
    if (to_push->RDLR & 0x4) {
      controls_allowed = 0;
    }
  }
}

// if controls_allowed and no pedals pressed
//     allow steering up to limit
// else
//     block all commands that produce actuation

static int chrysler_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {
  // There can be only one! (LKAS)
  if (chrysler_lkas_detected) {
    return 0;
  }

  uint32_t addr;
  if (to_send->RIR & 4) {
    // Extended
    addr = to_send->RIR >> 3;
  } else {
    // Normal
    addr = to_send->RIR >> 21;
  }

  // LKA STEER: Too large of values are ignored by the car, but still filter it out.
  if (addr == 0x292) {
    int rdlr = to_send->RDLR;
    int steer = ((rdlr & 0x7) << 8) + ((rdlr & 0xFF00) >> 8);
    int straight = 1024;
    int max_steer = 250;
    if (steer > (straight + max_steer)) {
      return false;
    }
    if (steer < (straight - max_steer)) {
      return false;
    }
    if (!controls_allowed) {
      return false;
    }
  }

  // 1 allows the message through
  return true;
}

static int chrysler_tx_lin_hook(int lin_num, uint8_t *data, int len) {
  // LIN is not used.
  return false;
}

static void chrysler_init(int16_t param) {
  controls_allowed = 0;
  chrysler_ignition_started = 0;
}

static int chrysler_ign_hook() {
  return chrysler_ignition_started;
}

static int chrysler_fwd_hook(int bus_num, CAN_FIFOMailBox_TypeDef *to_fwd) {
  return -1;
}

const safety_hooks chrysler_hooks = {
  .init = chrysler_init,
  .rx = chrysler_rx_hook,
  .tx = chrysler_tx_hook,
  .tx_lin = chrysler_tx_lin_hook,
  .ignition = chrysler_ign_hook,
  .fwd = chrysler_fwd_hook,
};

