const int VOLKSWAGEN_PQ_MAX_STEER = 300;                // 3.0 Nm (EPS side max of 3.0Nm with fault if violated)
const int VOLKSWAGEN_PQ_MAX_RT_DELTA = 188;             // 10 max rate up * 50Hz send rate * 250000 RT interval / 1000000 = 125 ; 125 * 1.5 for safety pad = 188
const uint32_t VOLKSWAGEN_PQ_RT_INTERVAL = 250000;      // 250ms between real time checks
const int VOLKSWAGEN_PQ_MAX_RATE_UP = 10;               // 5.0 Nm/s RoC limit (EPS rack has own soft-limit of 5.0 Nm/s)
const int VOLKSWAGEN_PQ_MAX_RATE_DOWN = 10;             // 5.0 Nm/s RoC limit (EPS rack has own soft-limit of 5.0 Nm/s)
const int VOLKSWAGEN_PQ_DRIVER_TORQUE_ALLOWANCE = 60;
const int VOLKSWAGEN_PQ_DRIVER_TORQUE_FACTOR = 3;
const int VOLKSWAGEN_PQ_MAX_ACCEL = 2000;               // Max accel 2.0 m/s2
const int VOLKSWAGEN_PQ_MIN_ACCEL = -3500;              // Max decel 3.5 m/s2

#define MSG_LENKHILFE_3         0x0D0   // RX from EPS, for steering angle and driver steering torque
#define MSG_HCA_1               0x0D2   // TX by OP, Heading Control Assist steering torque
#define MSG_BREMSE_1            0x1A0   // RX from ABS, for ego speed
#define MSG_MOTOR_2             0x288   // RX from ECU, for CC state and brake switch state
#define MSG_ACC_SYSTEM          0x368   // TX by OP, longitudinal acceleration controls
#define MSG_MOTOR_3             0x380   // RX from ECU, for driver throttle input
#define MSG_GRA_NEU             0x38A   // TX by OP, ACC control buttons for cancel/resume
#define MSG_MOTOR_5             0x480   // RX from ECU, for ACC main switch state
#define MSG_ACC_GRA_ANZIEGE     0x56A   // TX by OP, ACC HUD
#define MSG_LDW_1               0x5BE   // TX by OP, Lane line recognition and text alerts

// Transmit of GRA_Neu is allowed on bus 0 and 2 to keep compatibility with gateway and camera integration
const CanMsg VOLKSWAGEN_PQ_STOCK_TX_MSGS[] = {{MSG_HCA_1, 0, 5}, {MSG_GRA_NEU, 0, 4},
                                              {MSG_GRA_NEU, 2, 4}, {MSG_LDW_1, 0, 8}};
const CanMsg VOLKSWAGEN_PQ_LONG_TX_MSGS[] =  {{MSG_HCA_1, 0, 5}, {MSG_GRA_NEU, 0, 4},
                                              {MSG_GRA_NEU, 2, 4}, {MSG_LDW_1, 0, 8},
                                              {MSG_ACC_SYSTEM, 0, 8}, {MSG_ACC_GRA_ANZIEGE, 0, 8}};

AddrCheckStruct volkswagen_pq_addr_checks[] = {
  {.msg = {{MSG_LENKHILFE_3, 0, 6, .check_checksum = true, .max_counter = 15U, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_BREMSE_1, 0, 8, .check_checksum = false, .max_counter = 0U, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_MOTOR_2, 0, 8, .check_checksum = false, .max_counter = 0U, .expected_timestep = 20000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_MOTOR_3, 0, 8, .check_checksum = false, .max_counter = 0U, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_MOTOR_5, 0, 8, .check_checksum = true, .max_counter = 0U, .expected_timestep = 20000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_GRA_NEU, 0, 4, .check_checksum = true, .max_counter = 15U, .expected_timestep = 33000U}, { 0 }, { 0 }}},
};
#define VOLKSWAGEN_PQ_ADDR_CHECKS_LEN (sizeof(volkswagen_pq_addr_checks) / sizeof(volkswagen_pq_addr_checks[0]))
addr_checks volkswagen_pq_rx_checks = {volkswagen_pq_addr_checks, VOLKSWAGEN_PQ_ADDR_CHECKS_LEN};

const uint16_t FLAG_VOLKSWAGEN_LONG_CONTROL = 1;
bool volkswagen_pq_longitudinal = false;
bool volkswagen_pq_acc_main_on = false;
bool volkswagen_pq_set_prev = false;
bool volkswagen_pq_resume_prev = false;

static uint32_t volkswagen_pq_get_checksum(CANPacket_t *to_push) {
  int addr = GET_ADDR(to_push);
  checksum = 0;

  if (addr == MSG_MOTOR_5) {
    checksum = (uint8_t)GET_BYTE(to_push, 7);
  } else {
    checksum = (uint8_t)GET_BYTE(to_push, 0);
  }
}

static uint8_t volkswagen_pq_get_counter(CANPacket_t *to_push) {
  int addr = GET_ADDR(to_push);
  uint8_t counter = 0;

  if (addr == MSG_LENKHILFE_3) {
    counter = (uint8_t)(GET_BYTE(to_push, 1) & 0xF0U) >> 4;
  } else if (addr == MSG_GRA_NEU) {
    counter = (uint8_t)(GET_BYTE(to_push, 2) & 0xF0U) >> 4;
  } else {
    counter = 0U;
  }

  return counter;
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

#ifdef ALLOW_DEBUG
  volkswagen_pq_longitudinal = GET_FLAG(param, FLAG_VOLKSWAGEN_LONG_CONTROL);
#endif
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

    if (volkswagen_pq_longitudinal) {
      // Exit controls on leading edge of Cancel, otherwise, enter controls on falling edge of Set or Resume
      // ACC main switch must be on to enter, exit immediately on main switch off
      if (addr == MSG_MOTOR_5) {
        // Signal: Motor_5.GRA_Hauptschalter
        volkswagen_pq_acc_main_on = GET_BIT(to_push, 50U);
        controls_allowed &= volkswagen_pq_acc_main_on;
      }
      if (addr == MSG_GRA_NEU) {
        // Signal: GRA_Neu.GRA_Neu_Setzen
        // Signal: GRA_Neu.GRA_Neu_Recall
        bool set_button = GET_BIT(to_push, 16U);
        bool resume_button = GET_BIT(to_push, 17U);
        if (volkswagen_pq_acc_main_on && ((!set_button && volkswagen_pq_set_prev) || (!resume_button && volkswagen_pq_resume_prev))) {
          controls_allowed = 1;
        }
        volkswagen_pq_set_prev = set_button;
        volkswagen_pq_resume_prev = resume_button;
        // Signal: GRA_ACC_01.GRA_Abbrechen
        if (GET_BIT(to_push, 9U) == 1U) {
          controls_allowed = 0;
        }
      }
    } else {
      // Enter controls on rising edge of stock ACC, exit controls if stock ACC disengages
      if (addr == MSG_MOTOR_2) {
        // Signal: Motor_2.GRA_Status
        int acc_status = (GET_BYTE(to_push, 2) & 0xC0U) >> 6;
        int cruise_engaged = ((acc_status == 1) || (acc_status == 2)) ? 1 : 0;
        if (cruise_engaged && !cruise_engaged_prev) {
          controls_allowed = 1;
        }
        if (!cruise_engaged) {
          controls_allowed = 0;
        }
        cruise_engaged_prev = cruise_engaged;
      }
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
  int addr = GET_ADDR(to_send);
  int tx = 1;

  if (volkswagen_pq_longitudinal) {
    tx = msg_allowed(to_send, VOLKSWAGEN_PQ_LONG_TX_MSGS, sizeof(VOLKSWAGEN_PQ_LONG_TX_MSGS) / sizeof(VOLKSWAGEN_PQ_LONG_TX_MSGS[0]));
  } else {
    tx = msg_allowed(to_send, VOLKSWAGEN_PQ_STOCK_TX_MSGS, sizeof(VOLKSWAGEN_PQ_STOCK_TX_MSGS) / sizeof(VOLKSWAGEN_PQ_STOCK_TX_MSGS[0]));
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

    bool violation = false;
    uint32_t ts = microsecond_timer_get();

    if (controls_allowed) {
      // *** global torque limit check ***
      violation |= max_limit_check(desired_torque, VOLKSWAGEN_PQ_MAX_STEER, -VOLKSWAGEN_PQ_MAX_STEER);

      // *** torque rate limit check ***
      violation |= driver_limit_check(desired_torque, desired_torque_last, &torque_driver,
        VOLKSWAGEN_PQ_MAX_STEER, VOLKSWAGEN_PQ_MAX_RATE_UP, VOLKSWAGEN_PQ_MAX_RATE_DOWN,
        VOLKSWAGEN_PQ_DRIVER_TORQUE_ALLOWANCE, VOLKSWAGEN_PQ_DRIVER_TORQUE_FACTOR);
      desired_torque_last = desired_torque;

      // *** torque real time rate limit check ***
      violation |= rt_rate_limit_check(desired_torque, rt_torque_last, VOLKSWAGEN_PQ_MAX_RT_DELTA);

      // every RT_INTERVAL set the new limits
      uint32_t ts_elapsed = get_ts_elapsed(ts, ts_last);
      if (ts_elapsed > VOLKSWAGEN_PQ_RT_INTERVAL) {
        rt_torque_last = desired_torque;
        ts_last = ts;
      }
    }

    // no torque if controls is not allowed
    if (!controls_allowed && (desired_torque != 0)) {
      violation = true;
    }

    // reset to 0 if either controls is not allowed or there's a violation
    if (violation || !controls_allowed) {
      desired_torque_last = 0;
      rt_torque_last = 0;
      ts_last = ts;
    }

    if (violation) {
      tx = 0;
    }
  }

  // Safety check for acceleration commands
  // To avoid floating point math, scale upward and compare to pre-scaled safety m/s2 boundaries
  if (addr == MSG_ACC_SYSTEM) {
    bool violation = 0;
    int desired_accel = 0;

    // Signal: ACC_System.ACS_Sollbeschl (acceleration in m/s2, scale 0.005, offset -7.22)
    desired_accel = ((((GET_BYTE(to_send, 4) & 0x7U) << 8) | GET_BYTE(to_send, 3)) * 5U) - 7220U;

    // VW send one increment above the max range when inactive
    if (desired_accel == 3010) {
      desired_accel = 0;
    }

    if (!longitudinal_allowed && (desired_accel != 0)) {
      violation = 1;
    }

    violation |= max_limit_check(desired_accel, VOLKSWAGEN_PQ_MAX_ACCEL, VOLKSWAGEN_PQ_MIN_ACCEL);

    if (violation) {
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
        // openpilot takes over LKAS steering control and related HUD messages from the camera
        bus_fwd = -1;
      } else if (volkswagen_pq_longitudinal && ((addr == MSG_ACC_SYSTEM) || (addr == MSG_ACC_GRA_ANZIEGE))) {
        // openpilot takes over acceleration/braking control and related HUD messages from the stock ACC radar
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
