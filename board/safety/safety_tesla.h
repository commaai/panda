#pragma once

#include "safety_declarations.h"

// TODO merge with non-Raven

static bool tesla_longitudinal = false;
static bool tesla_powertrain = false;  // Are we the second panda intercepting the powertrain bus?
static bool tesla_raven = false;

static bool tesla_stock_aeb = false;

static void tesla_rx_hook(const CANPacket_t *to_push) {
  int bus = GET_BUS(to_push);
  int addr = GET_ADDR(to_push);

  if (!tesla_powertrain) {
    if((addr == 0x370) && (bus == 0)) {
      // Steering angle: (0.1 * val) - 819.2 in deg.
      // Store it 1/10 deg to match steering request
      int angle_meas_new = (((GET_BYTE(to_push, 4) & 0x3FU) << 8) | GET_BYTE(to_push, 5)) - 8192U;
      update_sample(&angle_meas, angle_meas_new);
    }
  }

  if ((tesla_powertrain && (bus == 0) && (addr == 0x116)) ||
     (!tesla_powertrain && (bus == 1) && (addr == 0x118))) {
    // DI_torque2: DI_vehicleSpeed
    // Vehicle speed: ((0.05 * val) - 25) * MPH_TO_MPS
    float speed = (((((GET_BYTE(to_push, 3) & 0x0FU) << 8) | (GET_BYTE(to_push, 2))) * 0.05) - 25) * 0.447;
    vehicle_moving = ABS(speed) > 0.1;
    UPDATE_VEHICLE_SPEED(speed);
  }

  if ((tesla_powertrain && (bus == 0) && (addr == 0x106)) ||
     (!tesla_powertrain && (bus == 1) && (addr == 0x108))) {
    gas_pressed = (GET_BYTE(to_push, 6) != 0U);
  }

  if ((tesla_powertrain && (bus == 0) && (addr == 0x1f8)) ||
     (!tesla_powertrain && (bus == 1) && (addr == 0x20a))) {
    brake_pressed = (((GET_BYTE(to_push, 0) & 0x0CU) >> 2) != 1U);
  }

  if ((tesla_powertrain && (bus == 0) && (addr == 0x256)) ||
     (!tesla_powertrain && (bus == 1) && (addr == 0x368))) {
    // Cruise state
    int cruise_state = (GET_BYTE(to_push, 1) >> 4);
    bool cruise_engaged = (cruise_state == 2) ||  // ENABLED
                          (cruise_state == 3) ||  // STANDSTILL
                          (cruise_state == 4) ||  // OVERRIDE
                          (cruise_state == 6) ||  // PRE_FAULT
                          (cruise_state == 7);    // PRE_CANCEL
    pcm_cruise_check(cruise_engaged);
  }

  if (bus == 2) {
    if (tesla_powertrain && tesla_longitudinal && (addr == 0x2bf)) {
      // "AEB_ACTIVE"
      tesla_stock_aeb = ((GET_BYTE(to_push, 2) & 0x03U) == 1U);
    }
  }

  if (tesla_powertrain) {
    // 0x2bf: DAS_control should not be received on bus 0
    generic_rx_checks((addr == 0x2bf) && (bus == 0));
  } else {
    // 0x488: DAS_steeringControl should not be received on bus 0
    generic_rx_checks((addr == 0x488) && (bus == 0));
  }
}


static bool tesla_tx_hook(const CANPacket_t *to_send) {
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
    .min_accel = 288,       // -3.48 m/s^2
    .inactive_accel = 375,  // 0. m/s^2
  };

  bool tx = true;
  int addr = GET_ADDR(to_send);
  bool violation = false;

  if(!tesla_powertrain && (addr == 0x488)) {
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

  if(tesla_powertrain && (addr == 0x2bf)) {
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
    // PARTY/PT to autopilot
    bus_fwd = 2;
  }

  if(bus_num == 2) {
    // Autopilot to PARTY/PT
    bool block_msg = false;
    if (!tesla_powertrain && (addr == 0x488)) {
      block_msg = true;
    }

    if (!tesla_powertrain && (addr == 0x27d)) {
      block_msg = true;
    }

    if (tesla_powertrain && tesla_longitudinal && (addr == 0x2bf) && !tesla_stock_aeb) {
      block_msg = true;
    }

    if(!block_msg) {
      bus_fwd = 0;
    }
  }

  return bus_fwd;
}

static safety_config tesla_init(uint16_t param) {
  const int TESLA_FLAG_POWERTRAIN = 1;
  const int TESLA_FLAG_LONGITUDINAL_CONTROL = 2;
  const int TESLA_FLAG_RAVEN = 4;

  static const CanMsg TESLA_TX_MSGS[] = {
    {0x488, 0, 4},  // DAS_steeringControl
    {0x27D, 0, 3},  // APS_eacMonitor
  };

  static const CanMsg TESLA_PT_TX_MSGS[] = {
    {0x2bf, 0, 8},  // DAS_control
  };

  tesla_powertrain = GET_FLAG(param, TESLA_FLAG_POWERTRAIN);
  tesla_longitudinal = GET_FLAG(param, TESLA_FLAG_LONGITUDINAL_CONTROL);
  tesla_raven = GET_FLAG(param, TESLA_FLAG_RAVEN);

  tesla_stock_aeb = false;

  safety_config ret;
  if (tesla_powertrain) {
    static RxCheck tesla_pt_rx_checks[] = {
      {.msg = {{0x106, 0, 8, .frequency = 100U}, { 0 }, { 0 }}},  // DI_torque1
      {.msg = {{0x116, 0, 6, .frequency = 100U}, { 0 }, { 0 }}},  // DI_torque2
      {.msg = {{0x1f8, 0, 8, .frequency = 50U}, { 0 }, { 0 }}},   // BrakeMessage
      {.msg = {{0x2bf, 2, 8, .frequency = 25U}, { 0 }, { 0 }}},   // DAS_control
      {.msg = {{0x256, 0, 8, .frequency = 10U}, { 0 }, { 0 }}},   // DI_state
    };

    ret = BUILD_SAFETY_CFG(tesla_pt_rx_checks, TESLA_PT_TX_MSGS);
  } else {
    static RxCheck tesla_raven_rx_checks[] = {
      {.msg = {{0x370, 0, 8, .frequency = 100U}, { 0 }, { 0 }}},  // EPAS3P_sysStatus
      {.msg = {{0x108, 1, 8, .frequency = 100U}, { 0 }, { 0 }}},  // DI_torque1
      {.msg = {{0x118, 1, 6, .frequency = 100U}, { 0 }, { 0 }}},  // DI_torque2
      {.msg = {{0x20a, 1, 8, .frequency = 50U}, { 0 }, { 0 }}},   // BrakeMessage
      {.msg = {{0x368, 1, 8, .frequency = 10U}, { 0 }, { 0 }}},   // DI_state
    };

    ret = BUILD_SAFETY_CFG(tesla_raven_rx_checks, TESLA_TX_MSGS);
  }
  return ret;
}

const safety_hooks tesla_hooks = {
  .init = tesla_init,
  .rx = tesla_rx_hook,
  .tx = tesla_tx_hook,
  .fwd = tesla_fwd_hook,
};
