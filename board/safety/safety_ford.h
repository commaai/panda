const struct lookup_t FORD_LOOKUP_ANGLE_RATE_UP = {
  {2., 7., 17.},
  {5., .8, .25}};

const struct lookup_t FORD_LOOKUP_ANGLE_RATE_DOWN = {
  {2., 7., 17.},
  {5., 3.5, .8}};

#define DEG_TO_RAD (3.14159265358979 / 180.0)
#define RAD_TO_DEG (180.0 / 3.14159265358979)
#define KPH_TO_MS (1.0 / 3.6)

// Signal: LatCtlPath_An_Actl
// Factor: 0.0005
#define FORD_RAD_TO_CAN 2000.0
#define FORD_DEG_TO_CAN DEG_TO_RAD * FORD_RAD_TO_CAN

// Safety-relevant CAN messages for Ford vehicles.
#define MSG_STEERING_PINION_DATA      0x07E  // RX from PSCM, for steering pinion angle
#define MSG_ENG_BRAKE_DATA            0x165  // RX from PCM, for driver brake pedal and cruise state
#define MSG_ENG_VEHICLE_SP_THROTTLE2  0x202  // RX from PCM, for vehicle speed
#define MSG_ENG_VEHICLE_SP_THROTTLE   0x204  // RX from PCM, for driver throttle input
#define MSG_STEERING_DATA_FD1         0x083  // TX by OP, ACC control buttons for cancel
#define MSG_ACC_DATA_3                0x18A  // TX by OP, IPMA ACC/TJA status
#define MSG_LANE_ASSIST_DATA1         0x3CA  // TX by OP, Lane Keeping Assist
#define MSG_LATERAL_MOTION_CONTROL    0x3D3  // TX by OP, Lane Centering Assist
#define MSG_IPMA_DATA                 0x3D8  // TX by OP, IPMA LKAS status

// CAN bus numbers.
#define FORD_MAIN 0U
#define FORD_CAM  2U

const CanMsg FORD_TX_MSGS[] = {
  {MSG_STEERING_DATA_FD1, 0, 8},
  {MSG_ACC_DATA_3, 0, 8},
  {MSG_LANE_ASSIST_DATA1, 0, 8},
  {MSG_LATERAL_MOTION_CONTROL, 0, 8},
  {MSG_IPMA_DATA, 0, 8},
};
#define FORD_TX_LEN (sizeof(FORD_TX_MSGS) / sizeof(FORD_TX_MSGS[0]))

AddrCheckStruct ford_addr_checks[] = {
  {.msg = {{MSG_STEERING_PINION_DATA, 0, 8, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_ENG_BRAKE_DATA, 0, 8, .expected_timestep = 100000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_ENG_VEHICLE_SP_THROTTLE2, 0, 8, .expected_timestep = 20000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_ENG_VEHICLE_SP_THROTTLE, 0, 8, .expected_timestep = 10000U}, { 0 }, { 0 }}},
};
#define FORD_ADDR_CHECK_LEN (sizeof(ford_addr_checks) / sizeof(ford_addr_checks[0]))
addr_checks ford_rx_checks = {ford_addr_checks, FORD_ADDR_CHECK_LEN};


static const addr_checks* ford_init(uint32_t param) {
  UNUSED(param);

  controls_allowed = false;
  relay_malfunction_reset();

  return &ford_rx_checks;
}

static int ford_rx_hook(CANPacket_t *to_push) {
  bool valid = addr_safety_check(to_push, &ford_rx_checks, NULL, NULL, NULL);

  if (valid && (GET_BUS(to_push) == FORD_MAIN)) {
    int addr = GET_ADDR(to_push);

    // Update steering angle
    if (addr == MSG_STEERING_PINION_DATA) {
      // Steering angle: (0.1 * val) - 1600 in deg
      // Signal: StePinComp_An_Est
      // Convert to CAN units (1/2000 radians).
      int angle_meas_new = ((((GET_BYTE(to_push, 2) & 0x7FU) << 7) | GET_BYTE(to_push, 3)) * 0.1) - 1600U;
      angle_meas_new *= FORD_DEG_TO_CAN;
      update_sample(&angle_meas, angle_meas_new);
    }

    // Update in motion state from speed value
    if (addr == MSG_ENG_VEHICLE_SP_THROTTLE2) {
      // Speed in km/h, convert to m/s
      // Speed: (0.01 * val) * KPH_TO_MS
      // Signal: Veh_V_ActlEng
      vehicle_speed = (((GET_BYTE(to_push, 6) << 8) | GET_BYTE(to_push, 7)) * 0.01) * KPH_TO_MS;
      vehicle_moving = vehicle_speed > 0.3;
    }

    // Update gas pedal
    if (addr == MSG_ENG_VEHICLE_SP_THROTTLE) {
      // Pedal position: (0.1 * val) in percent
      // Signal: ApedPos_Pc_ActlArb
      gas_pressed = (((GET_BYTE(to_push, 0) & 0x03U) << 8) | GET_BYTE(to_push, 1)) != 0U;
    }

    // Update brake pedal and cruise state
    if (addr == MSG_ENG_BRAKE_DATA) {
      // Signal: BpedDrvAppl_D_Actl
      brake_pressed = ((GET_BYTE(to_push, 0) & 0x30U) >> 4) == 2U;

      // Signal: CcStat_D_Actl
      bool cruise_engaged = (GET_BYTE(to_push, 1) & 0x07U) == 5U;

      // Enter controls on rising edge of stock ACC, exit controls if stock ACC disengages
      if (cruise_engaged && !cruise_engaged_prev) {
        controls_allowed = true;
      }
      if (!cruise_engaged) {
        controls_allowed = false;
      }
      cruise_engaged_prev = cruise_engaged;
    }

    // If steering controls messages are received on the destination bus, it's an indication
    // that the relay might be malfunctioning.
    bool is_lkas_msg = (addr == MSG_LANE_ASSIST_DATA1)
                    || (addr == MSG_LATERAL_MOTION_CONTROL)
                    || (addr == MSG_IPMA_DATA);
    generic_rx_checks(is_lkas_msg);
  }

  return valid;
}

static bool ford_steering_check(int desired_angle) {
  // Add 1 to not false trigger violation
  float delta_angle_float;
  delta_angle_float = (interpolate(FORD_LOOKUP_ANGLE_RATE_UP, vehicle_speed) * FORD_DEG_TO_CAN);
  int delta_angle_up = (int)(delta_angle_float) + 1;
  delta_angle_float = (interpolate(FORD_LOOKUP_ANGLE_RATE_DOWN, vehicle_speed) * FORD_DEG_TO_CAN);
  int delta_angle_down = (int)(delta_angle_float) + 1;
  int highest_desired_angle = desired_angle_last + ((desired_angle_last > 0) ? delta_angle_up : delta_angle_down);
  int lowest_desired_angle = desired_angle_last - ((desired_angle_last >= 0) ? delta_angle_down : delta_angle_up);

  // Check for violation
  return max_limit_check(desired_angle, highest_desired_angle, lowest_desired_angle);
}

static int ford_tx_hook(CANPacket_t *to_send, bool longitudinal_allowed) {
  UNUSED(longitudinal_allowed);

  int tx = 1;
  int addr = GET_ADDR(to_send);

  if (!msg_allowed(to_send, FORD_TX_MSGS, FORD_TX_LEN)) {
    tx = 0;
  }

  // Cruise button check, only allow cancel button to be sent
  if (addr == MSG_STEERING_DATA_FD1) {
    // Violation if any button other than cancel is pressed
    // Signal: CcAslButtnCnclPress
    bool violation = 0U != (GET_BYTE(to_send, 0) |
                            (GET_BYTE(to_send, 1) & 0xFEU) |
                            GET_BYTE(to_send, 2) |
                            GET_BYTE(to_send, 3) |
                            GET_BYTE(to_send, 4) |
                            GET_BYTE(to_send, 5));
    if (violation) {
      tx = 0;
    }
  }

  // Safety check for Lane_Assist_Data1 action
  if (addr == MSG_LANE_ASSIST_DATA1) {
    // Do not allow steering using Lane_Assist_Data1 (Lane-Departure Aid).
    // This message must be sent for Lane Centering to work, and can include
    // values such as the steering angle or lane curvature for debugging,
    // but the action must be set to zero.
    unsigned int action = (GET_BYTE(to_send, 0) & 0xE0U) >> 5;
    if (action != 0U) {
      tx = 0;
    }
  }

  // Safety check for LateralMotionControl steering angle
  if (addr == MSG_LATERAL_MOTION_CONTROL) {
    // Signal: LatCtlPath_An_Actl
    // Command Angle: (0.0005 * val) - 0.5 in radians
    // Store in CAN units (1/2000 or 0.0005 radians).
    int desired_angle = ((GET_BYTE(to_send, 3) << 3) | (GET_BYTE(to_send, 4) >> 5)) - 1000U;

    // Signal: LatCtl_D_Rq
    unsigned int steer_control_type = (GET_BYTE(to_send, 4) & 0x1CU) >> 2;
    bool steer_control_enabled = steer_control_type != 0U;

    // Rate limit while steering
    if (controls_allowed && steer_control_enabled) {
      if (ford_steering_check(desired_angle)) {
        tx = 0;
      }
    }
    desired_angle_last = desired_angle;

    // No steer control allowed when controls are not allowed
    if (!controls_allowed && steer_control_enabled) {
      tx = 0;
    }

    // Reset to 0 if controls not allowed
    if (!controls_allowed) {
      desired_angle_last = 0;
    }
  }

  // 1 allows the message through
  return tx;
}

static int ford_fwd_hook(int bus_num, CANPacket_t *to_fwd) {
  int addr = GET_ADDR(to_fwd);
  int bus_fwd = -1;

  switch (bus_num) {
    case FORD_MAIN: {
      // Forward all traffic from bus 0 onward
      bus_fwd = FORD_CAM;
      break;
    }
    case FORD_CAM: {
      // Block stock LKAS messages
      bool is_lkas_msg = (addr == MSG_ACC_DATA_3)
                      || (addr == MSG_LANE_ASSIST_DATA1)
                      || (addr == MSG_LATERAL_MOTION_CONTROL)
                      || (addr == MSG_IPMA_DATA);
      if (!is_lkas_msg) {
        bus_fwd = FORD_MAIN;
      }
      break;
    }
    default: {
      // No other buses should be in use; fallback to do-not-forward
      bus_fwd = -1;
      break;
    }
  }

  return bus_fwd;
}

const safety_hooks ford_hooks = {
  .init = ford_init,
  .rx = ford_rx_hook,
  .tx = ford_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = ford_fwd_hook,
};
