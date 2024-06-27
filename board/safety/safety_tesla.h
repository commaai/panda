const SteeringLimits TESLA_STEERING_LIMITS = {
  .angle_deg_to_can = 10,
  .angle_rate_up_lookup = {
    {0., 5., 15.},
    {10., 1.6, .3}
  },
  .angle_rate_down_lookup = {
    {0., 5., 15.},
    {10., 7.0, .8}
  },
};

const LongitudinalLimits TESLA_LONG_LIMITS = {
  .max_accel = 425,       // 2. m/s^2
  .min_accel = 287,       // -3.52 m/s^2  // TODO: limit to -3.48
  .inactive_accel = 375,  // 0. m/s^2
};

const int TESLA_FLAG_MODEL3_Y = 1;
const int TESLA_FLAG_LONGITUDINAL_CONTROL = 2;

const CanMsg TESLA_M3_Y_TX_MSGS[] = {
  {0x488, 0, 4},  // DAS_steeringControl
  {0x2b9, 0, 8},  // DAS_control
  {0x229, 1, 3},  // SCCM_rightStalk
};

RxCheck tesla_model3_y_rx_checks[] = {
  {.msg = {{0x2b9, 2, 8, .frequency = 25U}, { 0 }, { 0 }}},   // DAS_control
  {.msg = {{0x155, 0, 8, .frequency = 50U}, { 0 }, { 0 }}},   // ESP_wheelRotation (speed in kph)
  {.msg = {{0x370, 0, 8, .frequency = 100U}, { 0 }, { 0 }}},  // EPAS3S_internalSAS (steering angle)
  {.msg = {{0x118, 0, 8, .frequency = 100U}, { 0 }, { 0 }}},  // DI_systemStatus (gas pedal)
  {.msg = {{0x39d, 0, 5, .frequency = 25U}, { 0 }, { 0 }}},   // IBST_status (brakes)
  {.msg = {{0x286, 0, 8, .frequency = 10U}, { 0 }, { 0 }}},   // DI_state (acc state)
  {.msg = {{0x311, 0, 7, .frequency = 10U}, { 0 }, { 0 }}},   // UI_warning (buckle switch & doors)
  {.msg = {{0x3f5, 1, 8, .frequency = 10U}, { 0 }, { 0 }}},   // ID3F5VCFRONT_lighting (blinkers)
};

bool tesla_longitudinal = false;
bool tesla_stock_aeb = false;

static void tesla_rx_hook(const CANPacket_t *to_push) {
  int bus = GET_BUS(to_push);
  int addr = GET_ADDR(to_push);

  if((addr == 0x370) && (bus == 0)) {
    // Steering angle: (0.1 * val) - 819.2 in deg.
    // Store it 1/10 deg to match steering request
    int angle_meas_new = (((GET_BYTE(to_push, 4) & 0x3FU) << 8) | GET_BYTE(to_push, 5)) - 8192U;
    update_sample(&angle_meas, angle_meas_new);
  }

  if(bus == 0) {
    if(addr == 0x155){
      // Vehicle speed: (val * 0.5) * KPH_TO_MPS
      float speed = ((((GET_BYTE(to_push, 6) & 0x0FU) << 6) | (GET_BYTE(to_push, 5) >> 2)) * 0.5) * 0.277778;
      vehicle_moving = ABS(speed) > 0.1;
      UPDATE_VEHICLE_SPEED(speed);
    }

    // Gas pressed
    if(addr == 0x118){
      gas_pressed = (GET_BYTE(to_push, 4) != 0U);
    }

    // Brake pressed
    if(addr == 0x39d){
      brake_pressed = (GET_BYTE(to_push, 2) & 0x03U)  == 2U;
    }

    // Cruise state
    if(addr == 0x286) {
      int cruise_state = ((GET_BYTE(to_push, 1) << 1 ) >> 5);
      bool cruise_engaged = (cruise_state == 2) ||  // ENABLED
                            (cruise_state == 3) ||  // STANDSTILL
                            (cruise_state == 4) ||  // OVERRIDE
                            (cruise_state == 6) ||  // PRE_FAULT
                            (cruise_state == 7);    // PRE_CANCEL
      pcm_cruise_check(cruise_engaged);
    }
  }

  if (bus == 2) {
    if (tesla_longitudinal && (addr == 0x2b9)) {
      // "AEB_ACTIVE"
      tesla_stock_aeb = ((GET_BYTE(to_push, 2) & 0x03U) == 1U);
    }
  }

  generic_rx_checks((addr == 0x488) && (bus == 0));
  generic_rx_checks((addr == 0x2b9) && (bus == 0));

}


static bool tesla_tx_hook(const CANPacket_t *to_send) {
  bool tx = true;
  int addr = GET_ADDR(to_send);
  bool violation = false;

  if(addr == 0x488) {
    // Steering control: (0.1 * val) - 1638.35 in deg.
    // We use 1/10 deg as a unit here
    int raw_angle_can = (((GET_BYTE(to_send, 0) & 0x7FU) << 8) | GET_BYTE(to_send, 1));
    int desired_angle = raw_angle_can - 16384;
    int steer_control_type = GET_BYTE(to_send, 2) >> 6;
    bool steer_control_enabled = (steer_control_type != 0) &&  // NONE
                                 (steer_control_type != 3);    // DISABLED

    if (steer_angle_cmd_checks(desired_angle, steer_control_enabled, TESLA_STEERING_LIMITS)) {
      violation = true;
    }
  }


  if (addr == 0x229){
    // Only the "Half up" and "Neutral" positions are permitted for sending stalk signals.
    int control_lever_status = ((GET_BYTE(to_send, 1) & 0x70U) >> 4);
    if ((control_lever_status > 1)) {
      violation = true;
    }
  }

  if(addr == 0x2b9) {
    // DAS_control: longitudinal control message
    if (tesla_longitudinal) {
      // No AEB events may be sent by openpilot
      int aeb_event = GET_BYTE(to_send, 2) & 0x03U;
      if (aeb_event != 0) {
        violation = true;
      }

      // Don't send messages when the stock AEB system is active
      if (tesla_stock_aeb) {
        violation = true;
      }

      // Don't allow any acceleration limits above the safety limits
      int raw_accel_max = ((GET_BYTE(to_send, 6) & 0x1FU) << 4) | (GET_BYTE(to_send, 5) >> 4);
      int raw_accel_min = ((GET_BYTE(to_send, 5) & 0x0FU) << 5) | (GET_BYTE(to_send, 4) >> 3);
      violation |= longitudinal_accel_checks(raw_accel_max, TESLA_LONG_LIMITS);
      violation |= longitudinal_accel_checks(raw_accel_min, TESLA_LONG_LIMITS);
    } else {
      violation = true;
    }
  }

  if (violation) {
    tx = false;
  }

  return tx;
}

static int tesla_fwd_hook(int bus_num, int addr) {
  int bus_fwd = -1;

  if(bus_num == 0) {
    // Party to autopilot
    bus_fwd = 2;
  }

  if(bus_num == 2) {
    bool block_msg = false;
    if (addr == 0x488) {
      block_msg = true;
    }

    if (tesla_longitudinal && (addr == 0x2b9) && !tesla_stock_aeb) {
      block_msg = true;
    }

    if(!block_msg) {
      bus_fwd = 0;
    }
  }

  return bus_fwd;
}

static safety_config tesla_init(uint16_t param) {
  tesla_longitudinal = GET_FLAG(param, TESLA_FLAG_LONGITUDINAL_CONTROL);
  tesla_stock_aeb = false;

  safety_config ret;
  ret = BUILD_SAFETY_CFG(tesla_model3_y_rx_checks, TESLA_M3_Y_TX_MSGS);
  return ret;
}

const safety_hooks tesla_hooks = {
  .init = tesla_init,
  .rx = tesla_rx_hook,
  .tx = tesla_tx_hook,
  .fwd = tesla_fwd_hook,
};
