const SteeringLimits VOLKSWAGEN_PQ_STEERING_LIMITS = {
  .max_steer = 300,                // 3.0 Nm (EPS side max of 3.0Nm with fault if violated)
  .max_rt_delta = 75,              // 4 max rate up * 50Hz send rate * 250000 RT interval / 1000000 = 50 ; 50 * 1.5 for safety pad = 75
  .max_rt_interval = 250000,       // 250ms between real time checks
  .max_rate_up = 4,                // 2.0 Nm/s RoC limit (EPS rack has own soft-limit of 5.0 Nm/s)
  .max_rate_down = 10,             // 5.0 Nm/s RoC limit (EPS rack has own soft-limit of 5.0 Nm/s)
  .driver_torque_factor = 3,
  .driver_torque_allowance = 80,
  .type = TorqueDriverLimited,
};

#define MSG_LENKHILFE_3 0x0D0   // RX from EPS, for steering angle and driver steering torque
#define MSG_HCA_1       0x0D2   // TX by OP, Heading Control Assist steering torque
#define MSG_MOTOR_2     0x288   // RX from ECU, for CC state and brake switch state
#define MSG_MOTOR_3     0x380   // RX from ECU, for driver throttle input
#define MSG_GRA_NEU     0x38A   // TX by OP, ACC control buttons for cancel/resume
#define MSG_BREMSE_1    0x1A0   // RX from ABS, for ego speed
#define MSG_LDW_1       0x5BE   // TX by OP, Lane line recognition and text alerts

// Transmit of GRA_Neu is allowed on bus 0 and 2 to keep compatibility with gateway and camera integration
const CanMsg VOLKSWAGEN_PQ_TX_MSGS[] = {{MSG_HCA_1, 0, 5}, {MSG_GRA_NEU, 0, 4}, {MSG_GRA_NEU, 2, 4}, {MSG_LDW_1, 0, 8}};
#define VOLKSWAGEN_PQ_TX_MSGS_LEN (sizeof(VOLKSWAGEN_PQ_TX_MSGS) / sizeof(VOLKSWAGEN_PQ_TX_MSGS[0]))

AddrCheckStruct volkswagen_pq_addr_checks[] = {
  {.msg = {{MSG_LENKHILFE_3, 0, 6, .check_checksum = true,  .max_counter = 15U, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_MOTOR_2, 0, 8, .check_checksum = false, .max_counter = 0U,  .expected_timestep = 20000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_MOTOR_3, 0, 8, .check_checksum = false, .max_counter = 0U,  .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_BREMSE_1, 0, 8, .check_checksum = false, .max_counter = 0U,  .expected_timestep = 10000U}, { 0 }, { 0 }}},
};
#define VOLKSWAGEN_PQ_ADDR_CHECKS_LEN (sizeof(volkswagen_pq_addr_checks) / sizeof(volkswagen_pq_addr_checks[0]))
addr_checks volkswagen_pq_rx_checks = {volkswagen_pq_addr_checks, VOLKSWAGEN_PQ_ADDR_CHECKS_LEN};


static uint32_t volkswagen_pq_get_checksum(CANPacket_t *to_push) {
  return (uint8_t)GET_BYTE(to_push, 0);
}

static uint8_t volkswagen_pq_get_counter(CANPacket_t *to_push) {
  // Few PQ messages have counters, and their offsets are inconsistent. This
  // function works only for Lenkhilfe_3 at this time.
  return (uint8_t)(GET_BYTE(to_push, 1) & 0xF0U) >> 4;
}

static uint32_t volkswagen_pq_compute_checksum(CANPacket_t *to_push) {
  int len = GET_LEN(to_push);
  uint8_t checksum = 0U;

  for (int i = 1; i < len; i++) {
    checksum ^= (uint8_t)GET_BYTE(to_push, i);
  }

  return checksum;
}

static const addr_checks* volkswagen_pq_init(uint16_t param) {
  UNUSED(param);

  return &volkswagen_pq_rx_checks;
}

static int volkswagen_pq_rx_hook(CANPacket_t *to_push) {

  bool valid = addr_safety_check(to_push, &volkswagen_pq_rx_checks,
                                volkswagen_pq_get_checksum, volkswagen_pq_compute_checksum, volkswagen_pq_get_counter);

  if (valid && (GET_BUS(to_push) == 0U)) {
    int addr = GET_ADDR(to_push);

    // Update in-motion state from speed value.
    // Signal: Bremse_1.Geschwindigkeit_neu__Bremse_1_
    if (addr == MSG_BREMSE_1) {
      int speed = ((GET_BYTE(to_push, 2) & 0xFEU) >> 1) | (GET_BYTE(to_push, 3) << 7);
      // DBC speed scale 0.01: 0.3m/s = 108.
      vehicle_moving = speed > 108;
    }

    // Update driver input torque samples
    // Signal: Lenkhilfe_3.LH3_LM (absolute torque)
    // Signal: Lenkhilfe_3.LH3_LMSign (direction)
    if (addr == MSG_LENKHILFE_3) {
      int torque_driver_new = GET_BYTE(to_push, 2) | ((GET_BYTE(to_push, 3) & 0x3U) << 8);
      int sign = (GET_BYTE(to_push, 3) & 0x4U) >> 2;
      if (sign == 1) {
        torque_driver_new *= -1;
      }
      update_sample(&torque_driver, torque_driver_new);
    }

    // Enter controls on rising edge of stock ACC, exit controls if stock ACC disengages
    // Signal: Motor_2.GRA_Status
    if (addr == MSG_MOTOR_2) {
      int acc_status = (GET_BYTE(to_push, 2) & 0xC0U) >> 6;
      bool cruise_engaged = (acc_status == 1) || (acc_status == 2);
      pcm_cruise_check(cruise_engaged);
    }

    // Signal: Motor_3.Fahrpedal_Rohsignal
    if (addr == MSG_MOTOR_3) {
      gas_pressed = (GET_BYTE(to_push, 2));
    }

    // Signal: Motor_2.Bremslichtschalter
    if (addr == MSG_MOTOR_2) {
      brake_pressed = (GET_BYTE(to_push, 2) & 0x1U);
    }

    generic_rx_checks((addr == MSG_HCA_1));
  }
  return valid;
}

static int volkswagen_pq_tx_hook(CANPacket_t *to_send, bool longitudinal_allowed) {
  UNUSED(longitudinal_allowed);

  int addr = GET_ADDR(to_send);
  int tx = 1;

  if (!msg_allowed(to_send, VOLKSWAGEN_PQ_TX_MSGS, VOLKSWAGEN_PQ_TX_MSGS_LEN)) {
    tx = 0;
  }

  // Safety check for HCA_1 Heading Control Assist torque
  // Signal: HCA_1.LM_Offset (absolute torque)
  // Signal: HCA_1.LM_Offsign (direction)
  if (addr == MSG_HCA_1) {
    int desired_torque = GET_BYTE(to_send, 2) | ((GET_BYTE(to_send, 3) & 0x7FU) << 8);
    desired_torque = desired_torque / 32;  // DBC scale from PQ network to centi-Nm
    int sign = (GET_BYTE(to_send, 3) & 0x80U) >> 7;
    if (sign == 1) {
      desired_torque *= -1;
    }

    if (steer_torque_cmd_checks(desired_torque, -1, VOLKSWAGEN_PQ_STEERING_LIMITS)) {
      tx = 0;
    }
  }

  // FORCE CANCEL: ensuring that only the cancel button press is sent when controls are off.
  // This avoids unintended engagements while still allowing resume spam
  if ((addr == MSG_GRA_NEU) && !controls_allowed) {
    // disallow resume and set: bits 16 and 17
    if ((GET_BYTE(to_send, 2) & 0x3U) != 0U) {
      tx = 0;
    }
  }

  // 1 allows the message through
  return tx;
}

static int volkswagen_pq_fwd_hook(int bus_num, CANPacket_t *to_fwd) {
  int addr = GET_ADDR(to_fwd);
  int bus_fwd = -1;

  switch (bus_num) {
    case 0:
      // Forward all traffic from the Extended CAN onward
      bus_fwd = 2;
      break;
    case 2:
      if ((addr == MSG_HCA_1) || (addr == MSG_LDW_1)) {
        // OP takes control of the Heading Control Assist and Lane Departure Warning messages from the camera
        bus_fwd = -1;
      } else {
        // Forward all remaining traffic from Extended CAN devices to J533 gateway
        bus_fwd = 0;
      }
      break;
    default:
      // No other buses should be in use; fallback to do-not-forward
      bus_fwd = -1;
      break;
  }

  return bus_fwd;
}

const safety_hooks volkswagen_pq_hooks = {
  .init = volkswagen_pq_init,
  .rx = volkswagen_pq_rx_hook,
  .tx = volkswagen_pq_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = volkswagen_pq_fwd_hook,
};
