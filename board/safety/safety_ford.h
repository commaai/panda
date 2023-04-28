// Safety-relevant CAN messages for Ford vehicles.
#define MSG_EngBrakeData          0x165   // RX from PCM, for driver brake pedal and cruise state
#define MSG_EngVehicleSpThrottle  0x204   // RX from PCM, for driver throttle input
#define MSG_DesiredTorqBrk        0x213   // RX from ABS, for standstill state
#define MSG_BrakeSysFeatures      0x415   // RX from ABS, for vehicle speed
#define MSG_EngVehicleSpThrottle2 0x202   // RX from PCM, for second vehicle speed
#define MSG_Yaw_Data_FD1          0x91    // RX from RCM, for yaw rate
#define MSG_Steering_Data_FD1     0x083   // TX by OP, various driver switches and LKAS/CC buttons
#define MSG_ACCDATA_3             0x18A   // TX by OP, ACC/TJA user interface
#define MSG_Lane_Assist_Data1     0x3CA   // TX by OP, Lane Keep Assist
#define MSG_LateralMotionControl  0x3D3   // TX by OP, Traffic Jam Assist
#define MSG_IPMA_Data             0x3D8   // TX by OP, IPMA and LKAS user interface

// CAN bus numbers.
#define FORD_MAIN_BUS 0U
#define FORD_CAM_BUS  2U

const CanMsg FORD_TX_MSGS[] = {
  {MSG_Steering_Data_FD1, 0, 8},
  {MSG_Steering_Data_FD1, 2, 8},
  {MSG_ACCDATA_3, 0, 8},
  {MSG_Lane_Assist_Data1, 0, 8},
  {MSG_LateralMotionControl, 0, 8},
  {MSG_IPMA_Data, 0, 8},
};
#define FORD_TX_LEN (sizeof(FORD_TX_MSGS) / sizeof(FORD_TX_MSGS[0]))

// warning: quality flags are not yet checked in openpilot's CAN parser,
// this may be the cause of blocked messages
AddrCheckStruct ford_addr_checks[] = {
  {.msg = {{MSG_BrakeSysFeatures, 0, 8, .check_checksum = true, .max_counter = 15U, .quality_flag=true, .expected_timestep = 20000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_EngVehicleSpThrottle2, 0, 8, .check_checksum = true, .max_counter = 15U, .quality_flag=true, .expected_timestep = 20000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_Yaw_Data_FD1, 0, 8, .check_checksum = true, .max_counter = 255U, .quality_flag=true, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  // These messages have no counter or checksum
  {.msg = {{MSG_EngBrakeData, 0, 8, .expected_timestep = 100000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_EngVehicleSpThrottle, 0, 8, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_DesiredTorqBrk, 0, 8, .expected_timestep = 20000U}, { 0 }, { 0 }}},
};
#define FORD_ADDR_CHECK_LEN (sizeof(ford_addr_checks) / sizeof(ford_addr_checks[0]))
addr_checks ford_rx_checks = {ford_addr_checks, FORD_ADDR_CHECK_LEN};

static uint8_t ford_get_counter(CANPacket_t *to_push) {
  int addr = GET_ADDR(to_push);

  uint8_t cnt;
  if (addr == MSG_BrakeSysFeatures) {
    // Signal: VehVActlBrk_No_Cnt
    cnt = (GET_BYTE(to_push, 2) >> 2) & 0xFU;
  } else if (addr == MSG_EngVehicleSpThrottle2) {
    // Signal: VehVActlEng_No_Cnt
    cnt = (GET_BYTE(to_push, 2) >> 3) & 0xFU;
  } else if (addr == MSG_Yaw_Data_FD1) {
    // Signal: VehRollYaw_No_Cnt
    cnt = GET_BYTE(to_push, 5);
  } else {
    cnt = 0;
  }
  return cnt;
}

static uint32_t ford_get_checksum(CANPacket_t *to_push) {
  int addr = GET_ADDR(to_push);

  uint8_t chksum;
  if (addr == MSG_BrakeSysFeatures) {
    // Signal: VehVActlBrk_No_Cs
    chksum = GET_BYTE(to_push, 3);
  } else if (addr == MSG_EngVehicleSpThrottle2) {
    // Signal: VehVActlEng_No_Cs
    chksum = GET_BYTE(to_push, 1);
  } else if (addr == MSG_Yaw_Data_FD1) {
    // Signal: VehRollYawW_No_Cs
    chksum = GET_BYTE(to_push, 4);
  } else {
    chksum = 0;
  }
  return chksum;
}

static uint32_t ford_compute_checksum(CANPacket_t *to_push) {
  int addr = GET_ADDR(to_push);

  uint8_t chksum = 0;
  if (addr == MSG_BrakeSysFeatures) {
    chksum += GET_BYTE(to_push, 0) + GET_BYTE(to_push, 1);  // Veh_V_ActlBrk
    chksum += GET_BYTE(to_push, 2) >> 6;                    // VehVActlBrk_D_Qf
    chksum += (GET_BYTE(to_push, 2) >> 2) & 0xFU;           // VehVActlBrk_No_Cnt
    chksum = 0xFFU - chksum;
  } else if (addr == MSG_EngVehicleSpThrottle2) {
    chksum += (GET_BYTE(to_push, 2) >> 3) & 0xFU;           // VehVActlEng_No_Cnt
    chksum += (GET_BYTE(to_push, 4) >> 5) & 0x3U;           // VehVActlEng_D_Qf
    chksum += GET_BYTE(to_push, 6) + GET_BYTE(to_push, 7);  // Veh_V_ActlEng
    chksum = 0xFFU - chksum;
  } else if (addr == MSG_Yaw_Data_FD1) {
    chksum += GET_BYTE(to_push, 0) + GET_BYTE(to_push, 1);  // VehRol_W_Actl
    chksum += GET_BYTE(to_push, 2) + GET_BYTE(to_push, 3);  // VehYaw_W_Actl
    chksum += GET_BYTE(to_push, 5);                         // VehRollYaw_No_Cnt
    chksum += GET_BYTE(to_push, 6) >> 6;                    // VehRolWActl_D_Qf
    chksum += (GET_BYTE(to_push, 6) >> 4) & 0x3U;           // VehYawWActl_D_Qf
    chksum = 0xFFU - chksum;
  } else {
  }

  return chksum;
}

static bool ford_get_quality_flag_valid(CANPacket_t *to_push) {
  int addr = GET_ADDR(to_push);

  bool valid = false;
  if (addr == MSG_BrakeSysFeatures) {
    valid = (GET_BYTE(to_push, 2) >> 6) == 0x3U;           // VehVActlBrk_D_Qf
  } else if (addr == MSG_EngVehicleSpThrottle2) {
    valid = ((GET_BYTE(to_push, 4) >> 5) & 0x3U) == 0x3U;  // VehVActlEng_D_Qf
  } else if (addr == MSG_Yaw_Data_FD1) {
    valid = (GET_BYTE(to_push, 6) >> 4) == 0xFU;           // VehRolWActl_D_Qf & VehYawWActl_D_Qf
  } else {
  }
  return valid;
}

#define INACTIVE_CURVATURE 1000U
#define INACTIVE_CURVATURE_RATE 4096U
#define INACTIVE_PATH_OFFSET 512U
#define INACTIVE_PATH_ANGLE 1000U
#define FORD_MAX_SPEED_DELTA 2.0  // m/s

static bool ford_lkas_msg_check(int addr) {
  return (addr == MSG_ACCDATA_3)
      || (addr == MSG_Lane_Assist_Data1)
      || (addr == MSG_LateralMotionControl)
      || (addr == MSG_IPMA_Data);
}

// Curvature rate limits
const SteeringLimits FORD_STEERING_LIMITS = {
  .max_steer = 1000,
  .angle_deg_to_can = 50000,        // 1 / (2e-5) rad to can
  .max_angle_error = 100,           // 0.002 * FORD_STEERING_LIMITS.angle_deg_to_can

  // no blending at low speed due to lack of torque wind-up and inaccurate current curvature
  .angle_error_limit_speed = 10.0,  // m/s

  .disable_angle_rate_limits = true,
  .enforce_angle_error = true,
  .inactive_angle_is_zero = true,
};

static int ford_rx_hook(CANPacket_t *to_push) {
  bool valid = addr_safety_check(to_push, &ford_rx_checks,
                                 ford_get_checksum, ford_compute_checksum, ford_get_counter, ford_get_quality_flag_valid);

  if (valid && (GET_BUS(to_push) == FORD_MAIN_BUS)) {
    int addr = GET_ADDR(to_push);

    // Update in motion state from standstill signal
    if (addr == MSG_DesiredTorqBrk) {
      // Signal: VehStop_D_Stat
      vehicle_moving = ((GET_BYTE(to_push, 3) >> 3) & 0x3U) == 0U;
    }

    // Update vehicle speed
    if (addr == MSG_BrakeSysFeatures) {
      // Signal: Veh_V_ActlBrk
      vehicle_speed = ((GET_BYTE(to_push, 0) << 8) | GET_BYTE(to_push, 1)) * 0.01 / 3.6;
    }

    // Check vehicle speed against a second source
    if (addr == MSG_EngVehicleSpThrottle2) {
      // Disable controls if speeds from ABS and PCM ECUs are too far apart.
      // Signal: Veh_V_ActlEng
      float filtered_pcm_speed = ((GET_BYTE(to_push, 6) << 8) | GET_BYTE(to_push, 7)) * 0.01 / 3.6;
      if (ABS(filtered_pcm_speed - vehicle_speed) > FORD_MAX_SPEED_DELTA) {
        controls_allowed = 0;
      }
    }

    // Update vehicle yaw rate
    if (addr == MSG_Yaw_Data_FD1) {
      // Signal: VehYaw_W_Actl
      float ford_yaw_rate = (((GET_BYTE(to_push, 2) << 8U) | GET_BYTE(to_push, 3)) * 0.0002) - 6.5;
      float current_curvature = ford_yaw_rate / MAX(vehicle_speed, 0.1);
      // convert current curvature into units on CAN for comparison with desired curvature
      int current_curvature_can = current_curvature * (float)FORD_STEERING_LIMITS.angle_deg_to_can +
                                  ((current_curvature > 0.) ? 0.5 : -0.5);
      update_sample(&angle_meas, current_curvature_can);
    }

    // Update gas pedal
    if (addr == MSG_EngVehicleSpThrottle) {
      // Pedal position: (0.1 * val) in percent
      // Signal: ApedPos_Pc_ActlArb
      gas_pressed = (((GET_BYTE(to_push, 0) & 0x03U) << 8) | GET_BYTE(to_push, 1)) > 0U;
    }

    // Update brake pedal and cruise state
    if (addr == MSG_EngBrakeData) {
      // Signal: BpedDrvAppl_D_Actl
      brake_pressed = ((GET_BYTE(to_push, 0) >> 4) & 0x3U) == 2U;

      // Signal: CcStat_D_Actl
      unsigned int cruise_state = GET_BYTE(to_push, 1) & 0x07U;
      bool cruise_engaged = (cruise_state == 4U) || (cruise_state == 5U);
      pcm_cruise_check(cruise_engaged);
    }

    // If steering controls messages are received on the destination bus, it's an indication
    // that the relay might be malfunctioning.
    generic_rx_checks(ford_lkas_msg_check(addr));
  }

  return valid;
}

static int ford_tx_hook(CANPacket_t *to_send) {

  int tx = 1;
  int addr = GET_ADDR(to_send);

  if (!msg_allowed(to_send, FORD_TX_MSGS, FORD_TX_LEN)) {
    tx = 0;
  }

  // Safety check for Steering_Data_FD1 button signals
  // Note: Many other signals in this message are not relevant to safety (e.g. blinkers, wiper switches, high beam)
  // which we passthru in OP.
  if (addr == MSG_Steering_Data_FD1) {
    // Violation if resume button is pressed while controls not allowed, or
    // if cancel button is pressed when cruise isn't engaged.
    bool violation = false;
    violation |= (GET_BIT(to_send, 8U) == 1U) && !cruise_engaged_prev;   // Signal: CcAslButtnCnclPress (cancel)
    violation |= (GET_BIT(to_send, 25U) == 1U) && !controls_allowed;     // Signal: CcAsllButtnResPress (resume)

    if (violation) {
      tx = 0;
    }
  }

  // Safety check for Lane_Assist_Data1 action
  if (addr == MSG_Lane_Assist_Data1) {
    // Do not allow steering using Lane_Assist_Data1 (Lane-Departure Aid).
    // This message must be sent for Lane Centering to work, and can include
    // values such as the steering angle or lane curvature for debugging,
    // but the action (LkaActvStats_D2_Req) must be set to zero.
    unsigned int action = GET_BYTE(to_send, 0) >> 5;
    if (action != 0U) {
      tx = 0;
    }
  }

  // Safety check for LateralMotionControl action
  if (addr == MSG_LateralMotionControl) {
    // Signal: LatCtl_D_Rq
    bool steer_control_enabled = ((GET_BYTE(to_send, 4) >> 2) & 0x7U) != 0U;
    unsigned int raw_curvature = (GET_BYTE(to_send, 0) << 3) | (GET_BYTE(to_send, 1) >> 5);
    unsigned int raw_curvature_rate = ((GET_BYTE(to_send, 1) & 0x1FU) << 8) | GET_BYTE(to_send, 2);
    unsigned int raw_path_angle = (GET_BYTE(to_send, 3) << 3) | (GET_BYTE(to_send, 4) >> 5);
    unsigned int raw_path_offset = (GET_BYTE(to_send, 5) << 2) | (GET_BYTE(to_send, 6) >> 6);

    // These signals are not yet tested with the current safety limits
    bool violation = (raw_curvature_rate != INACTIVE_CURVATURE_RATE) || (raw_path_angle != INACTIVE_PATH_ANGLE) || (raw_path_offset != INACTIVE_PATH_OFFSET);

    // Check angle error and steer_control_enabled
    int desired_curvature = raw_curvature - INACTIVE_CURVATURE;  // /FORD_STEERING_LIMITS.angle_deg_to_can to get real curvature
    violation |= steer_angle_cmd_checks(desired_curvature, steer_control_enabled, FORD_STEERING_LIMITS);

    if (violation) {
      tx = 0;
    }
  }

  // 1 allows the message through
  return tx;
}

static int ford_fwd_hook(int bus_num, int addr) {
  int bus_fwd = -1;

  switch (bus_num) {
    case FORD_MAIN_BUS: {
      // Forward all traffic from bus 0 onward
      bus_fwd = FORD_CAM_BUS;
      break;
    }
    case FORD_CAM_BUS: {
      // Block stock LKAS messages
      if (!ford_lkas_msg_check(addr)) {
        bus_fwd = FORD_MAIN_BUS;
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

static const addr_checks* ford_init(uint16_t param) {
  UNUSED(param);

  return &ford_rx_checks;
}

const safety_hooks ford_hooks = {
  .init = ford_init,
  .rx = ford_rx_hook,
  .tx = ford_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = ford_fwd_hook,
};
