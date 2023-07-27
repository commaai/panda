#include "safety_volkswagen_common.h"

// lateral limits
const SteeringLimits VOLKSWAGEN_MQBEVO_STEERING_LIMITS = {
  .max_steer = 370,              // TODO: true max TBD
  .max_rt_delta = 188,           // TODO: true max TBD, 10 max rate up * 50Hz send rate * 250000 RT interval / 1000000 = 125 ; 125 * 1.5 for safety pad = 187.5
  .max_rt_interval = 250000,     // 250ms between real time checks
  .max_rate_up = 10,             // TODO: true max TBD
  .max_rate_down = 10,           // TODO: true max TBD
  .driver_torque_allowance = 80,
  .driver_torque_factor = 3,
  .type = TorqueDriverLimited,
};

// Transmit of GRA_ACC_01 is allowed on bus 0 and 2 to keep compatibility with gateway and camera integration
const CanMsg VOLKSWAGEN_MQBEVO_STOCK_TX_MSGS[] = {{VW_MSG_HCA_NEW, 0, 24}, {VW_MSG_GRA_ACC_01, 0, 8},
                                                  {VW_MSG_GRA_ACC_01, 2, 8}};

AddrCheckStruct volkswagen_mqbevo_addr_checks[] = {
  // TODO: checksums for ESP_NEW_1, ESP_NEW_3, MOTOR_NEW_1
  {.msg = {{VW_MSG_ESP_NEW_1, 0, 48, .check_checksum = false, .max_counter = 0U, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{VW_MSG_LH_EPS_03, 0, 8, .check_checksum = true, .max_counter = 15U, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{VW_MSG_ESP_NEW_3, 0, 32, .check_checksum = false, .max_counter = 15U, .expected_timestep = 20000U}, { 0 }, { 0 }}},
  {.msg = {{VW_MSG_MOTOR_NEW_1, 0, 32, .check_checksum = false, .max_counter = 15U, .expected_timestep = 20000U}, { 0 }, { 0 }}},
  {.msg = {{VW_MSG_MOTOR_14, 0, 8, .check_checksum = false, .max_counter = 0U, .expected_timestep = 100000U}, { 0 }, { 0 }}},
};
#define VOLKSWAGEN_MQBEVO_ADDR_CHECKS_LEN (sizeof(volkswagen_mqbevo_addr_checks) / sizeof(volkswagen_mqbevo_addr_checks[0]))
addr_checks volkswagen_mqbevo_rx_checks = {volkswagen_mqbevo_addr_checks, VOLKSWAGEN_MQBEVO_ADDR_CHECKS_LEN};


static const addr_checks* volkswagen_mqbevo_init(uint16_t param) {
  volkswagen_common_init(param);
  return &volkswagen_mqbevo_rx_checks;
}

static int volkswagen_mqbevo_rx_hook(CANPacket_t *to_push) {
  bool valid = addr_safety_check(to_push, &volkswagen_mqbevo_rx_checks, volkswagen_mqb_get_checksum,
                                 volkswagen_mqb_compute_crc, volkswagen_mqb_get_counter, NULL);

  if (valid && (GET_BUS(to_push) == 0U)) {
    int addr = GET_ADDR(to_push);

    if (addr == VW_MSG_ESP_NEW_1) {
      // Update in-motion state by summing wheel speeds
      // Signals: ESP_NEW_1.WHEEL_SPEED_[FL,FR,RL,RR]
      int speed = 0;
      for (uint8_t i = 11U; i < 19U; i += 2U) {
        int wheel_speed = GET_BYTE(to_push, i) | (GET_BYTE(to_push, i + 1U) << 8);
        speed += wheel_speed;
      }
      // Check all wheel speeds for any movement
      vehicle_moving = speed > 0;
    }

    if (addr == VW_MSG_LH_EPS_03) {
      // Update driver input torque samples
      // Signal: LH_EPS_03.EPS_Lenkmoment (absolute torque)
      // Signal: LH_EPS_03.EPS_VZ_Lenkmoment (direction)
      int torque_driver_new = GET_BYTE(to_push, 5) | ((GET_BYTE(to_push, 6) & 0x1FU) << 8);
      int sign = (GET_BYTE(to_push, 6) & 0x80U) >> 7;
      if (sign == 1) {
        torque_driver_new *= -1;
      }
      update_sample(&torque_driver, torque_driver_new);
    }

    if (addr == VW_MSG_MOTOR_NEW_1) {
      // Update gas-pressed state
      // Signal: MOTOR_NEW_1.ACCEL_PEDAL
      gas_pressed = (((GET_BYTE(to_push, 1) & 0xF0U) >> 4) | ((GET_BYTE(to_push, 2) & 0xF) << 4));

      // When using stock ACC, enter controls on rising edge of stock ACC engage, exit on disengage
      // Signal: MOTOR_NEW_1.TSK_STATUS
      int acc_status = (GET_BYTE(to_push, 11) & 0x7U);
      bool cruise_engaged = (acc_status == 3) || (acc_status == 4) || (acc_status == 5);
      pcm_cruise_check(cruise_engaged);
    }

    if (addr == VW_MSG_GRA_ACC_01) {
      // Always exit controls on rising edge of Cancel
      // Signal: GRA_ACC_01.GRA_Abbrechen
      if (GET_BIT(to_push, 13U) == 1U) {
        controls_allowed = false;
      }
    }

    if (addr == VW_MSG_MOTOR_14) {
      // Signal: Motor_14.MO_Fahrer_bremst (ECU detected brake pedal switch F63)
      volkswagen_brake_pedal_switch = GET_BIT(to_push, 28U);
    }

    if (addr == VW_MSG_ESP_NEW_3) {
      // Signal: ESP_NEW_3.BRAKE_PRESSED_1 (ESP detected driver brake pressure above platform specified threshold)
      volkswagen_brake_pressure_detected = GET_BIT(to_push, 16U);
    }

    brake_pressed = volkswagen_brake_pedal_switch || volkswagen_brake_pressure_detected;
    generic_rx_checks((addr == VW_MSG_HCA_NEW));
  }

  return valid;
}

static int volkswagen_mqbevo_tx_hook(CANPacket_t *to_send) {
  int addr = GET_ADDR(to_send);
  int tx = 1;

  tx = msg_allowed(to_send, VOLKSWAGEN_MQBEVO_STOCK_TX_MSGS, sizeof(VOLKSWAGEN_MQBEVO_STOCK_TX_MSGS) / sizeof(VOLKSWAGEN_MQBEVO_STOCK_TX_MSGS[0]));

  if (addr == VW_MSG_HCA_NEW) {
    // Safety check for Heading Control Assist torque
    // Signal: HCA_NEW.ASSIST_TORQUE (absolute torque)
    // Signal: HCA_NEW.ASSIST_DIRECTION (direction)
    int desired_torque = GET_BYTE(to_send, 3) | ((GET_BYTE(to_send, 4) & 0x3U) << 8);
    bool sign = GET_BIT(to_send, 39U);
    if (sign) {
      desired_torque *= -1;
    }

    if (steer_torque_cmd_checks(desired_torque, -1, VOLKSWAGEN_MQBEVO_STEERING_LIMITS)) {
      tx = 0;
    }
  }

  if (addr == VW_MSG_GRA_ACC_01) {
    // Disallow resume and set while controls are disabled
    // Signal: GRA_ACC_01.GRA_Tip_Setzen
    // Signal: GRA_ACC_01.GRA_Tip_Wiederaufnahme
    if (!controls_allowed && (GET_BIT(to_send, 16) || GET_BIT(to_send, 19))) {
      tx = 0;
    }
  }

  return tx;
}

static int volkswagen_mqbevo_fwd_hook(int bus_num, int addr) {
  int bus_fwd = -1;

  switch (bus_num) {
    case 0:
      // Forward all traffic from the Extended CAN onward
      bus_fwd = 2;
      break;
    case 2:
      if ((addr == VW_MSG_HCA_NEW)) {
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

const safety_hooks volkswagen_mqbevo_hooks = {
  .init = volkswagen_mqbevo_init,
  .rx = volkswagen_mqbevo_rx_hook,
  .tx = volkswagen_mqbevo_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = volkswagen_mqbevo_fwd_hook,
};
