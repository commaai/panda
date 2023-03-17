#include "safety_volkswagen_common.h"

// lateral limits
const SteeringLimits VOLKSWAGEN_MLB_STEERING_LIMITS = {
  .max_steer = 300,              // 3.0 Nm (EPS side max of 3.0Nm with fault if violated)
  .max_rt_delta = 188,           // 10 max rate up * 50Hz send rate * 250000 RT interval / 1000000 = 125 ; 125 * 1.5 for safety pad = 187.5
  .max_rt_interval = 250000,     // 250ms between real time checks
  .max_rate_up = 10,             // 5.0 Nm/s RoC limit (EPS rack has own soft-limit of 5.0 Nm/s)
  .max_rate_down = 10,           // 5.0 Nm/s RoC limit (EPS rack has own soft-limit of 5.0 Nm/s)
  .driver_torque_allowance = 60,
  .driver_torque_factor = 3,
  .type = TorqueDriverLimited,
};

// longitudinal limits
// acceleration in m/s2 * 1000 to avoid floating point math
const LongitudinalLimits VOLKSWAGEN_MLB_LONG_LIMITS = {
  .max_accel = 2000,
  .min_accel = -3500,
  .inactive_accel = 3010,  // VW sends one increment above the max range when inactive
};

// Transmit of LS_01 is allowed on bus 0 and 2 to keep compatibility with gateway and camera integration
const CanMsg VOLKSWAGEN_MLB_STOCK_TX_MSGS[] = {{MSG_HCA_01, 0, 8}, {MSG_LS_01, 0, 4}, {MSG_LS_01, 2, 4}, {MSG_LDW_02, 0, 8}};

AddrCheckStruct volkswagen_mlb_addr_checks[] = {
  {.msg = {{MSG_ESP_03, 0, 8, .check_checksum = false, .max_counter = 15U, .expected_timestep = 20000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_LH_EPS_03, 0, 8, .check_checksum = false, .max_counter = 15U, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_ESP_05, 0, 8, .check_checksum = false, .max_counter = 15U, .expected_timestep = 20000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_TSK_02, 0, 8, .check_checksum = false, .max_counter = 15U, .expected_timestep = 30000U}, { 0 }, { 0 }}},
  {.msg = {{MSG_MOTOR_03, 0, 8, .check_checksum = false, .max_counter = 15U, .expected_timestep = 10000U}, { 0 }, { 0 }}},
};
#define VOLKSWAGEN_MLB_ADDR_CHECKS_LEN (sizeof(volkswagen_mlb_addr_checks) / sizeof(volkswagen_mlb_addr_checks[0]))
addr_checks volkswagen_mlb_rx_checks = {volkswagen_mlb_addr_checks, VOLKSWAGEN_MLB_ADDR_CHECKS_LEN};


static const addr_checks* volkswagen_mlb_init(uint16_t param) {
  UNUSED(param);

  volkswagen_brake_pedal_switch = false;
  volkswagen_brake_pressure_detected = false;

  gen_crc_lookup_table_8(0x2F, volkswagen_crc8_lut_8h2f);
  return &volkswagen_mlb_rx_checks;
}

static int volkswagen_mlb_rx_hook(CANPacket_t *to_push) {

  bool valid = addr_safety_check(to_push, &volkswagen_mlb_rx_checks, volkswagen_mlb_mqb_get_checksum,
                                 volkswagen_mlb_mqb_compute_checksum, volkswagen_mlb_mqb_get_counter, NULL);

  if (valid && (GET_BUS(to_push) == 0U)) {
    int addr = GET_ADDR(to_push);

    // Check all wheel speeds for any movement
    // Signals: ESP_03.ESP_[VL|VR|HL|HR]_Radgeschw
    if (addr == MSG_ESP_03) {
      int speed = 0;
      speed += GET_BYTE(to_push, 2) | ((GET_BYTE(to_push, 3) & 0xFU) << 8);         // FL
      speed += ((GET_BYTE(to_push, 3) & 0xF0) >> 4) | (GET_BYTE(to_push, 4) << 4);  // FR
      speed += GET_BYTE(to_push, 5) | ((GET_BYTE(to_push, 6) & 0xFU) << 8);         // RL
      speed += ((GET_BYTE(to_push, 6) & 0xF0) >> 4) | (GET_BYTE(to_push, 7) << 4);  // RR
      vehicle_moving = speed > 0;
    }

    // Update driver input torque
    if (addr == MSG_LH_EPS_03) {
      volkswagen_mlb_mqb_driver_input_torque(to_push);
    }

    if (addr == MSG_TSK_02) {
      // When using stock ACC, enter controls on rising edge of stock ACC engage, exit on disengage
      // Always exit controls on main switch off
      // Signal: TSK_02.TSK_Status
      int acc_status = (GET_BYTE(to_push, 2) & 0x3U);
      bool cruise_engaged = (acc_status == 1) || (acc_status == 2);
      acc_main_on = cruise_engaged || (acc_status == 0);  // FIXME: this is wrong

      pcm_cruise_check(cruise_engaged);

      if (!acc_main_on) {
        controls_allowed = false;
      }
    }

    if (addr == MSG_LS_01) {
      // Always exit controls on rising edge of Cancel
      // Signal: LS_01.LS_Abbrechen
      if (GET_BIT(to_push, 13U) == 1U) {
        controls_allowed = false;
      }
    }

    // Signal: Motor_03.MO_Fahrpedalrohwert_01
    // Signal: Motor_03.MO_Fahrer_bremst
    if (addr == MSG_MOTOR_03) {
      gas_pressed = GET_BYTE(to_push, 6) != 0U;
      volkswagen_brake_pedal_switch = GET_BIT(to_push, 35U);
    }

    if (addr == MSG_ESP_05) {
      volkswagen_mlb_mqb_brake_pressure_threshold(to_push);
    }

    brake_pressed = volkswagen_brake_pedal_switch || volkswagen_brake_pressure_detected;

    generic_rx_checks((addr == MSG_HCA_01));
  }
  return valid;
}

static int volkswagen_mlb_tx_hook(CANPacket_t *to_send) {
  int addr = GET_ADDR(to_send);
  int tx = 1;

  tx = msg_allowed(to_send, VOLKSWAGEN_MLB_STOCK_TX_MSGS, sizeof(VOLKSWAGEN_MLB_STOCK_TX_MSGS) / sizeof(VOLKSWAGEN_MLB_STOCK_TX_MSGS[0]));

  // Safety check for HCA_01 Heading Control Assist torque
  // Signal: HCA_01.Assist_Torque (absolute torque)
  // Signal: HCA_01.Assist_VZ (direction)
  if (addr == MSG_HCA_01) {
    int desired_torque = GET_BYTE(to_send, 2) | ((GET_BYTE(to_send, 3) & 0x3FU) << 8);
    int sign = (GET_BYTE(to_send, 3) & 0x80U) >> 7;
    if (sign == 1) {
      desired_torque *= -1;
    }

    if (steer_torque_cmd_checks(desired_torque, -1, VOLKSWAGEN_MLB_STEERING_LIMITS)) {
      tx = 0;
    }
  }

  // FORCE CANCEL: ensuring that only the cancel button press is sent when controls are off.
  // This avoids unintended engagements while still allowing resume spam
  if ((addr == MSG_LS_01) && !controls_allowed) {
    // disallow resume and set: bits 16 and 19
    if (GET_BIT(to_send, 16U) || GET_BIT(to_send, 19U)) {
      tx = 0;
    }
  }

  // 1 allows the message through
  return tx;
}

static int volkswagen_mlb_fwd_hook(int bus_num, CANPacket_t *to_fwd) {
  int addr = GET_ADDR(to_fwd);
  int bus_fwd = -1;

  switch (bus_num) {
    case 0:
      // Forward all traffic from the Extended CAN onward
      bus_fwd = 2;
      break;
    case 2:
      if ((addr == MSG_HCA_01) || (addr == MSG_LDW_02)) {
        // openpilot takes over LKAS steering control and related HUD messages from the camera
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

const safety_hooks volkswagen_mlb_hooks = {
  .init = volkswagen_mlb_init,
  .rx = volkswagen_mlb_rx_hook,
  .tx = volkswagen_mlb_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = volkswagen_mlb_fwd_hook,
};
