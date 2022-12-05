// CAN bus numbers
#define MAZDA_MAIN 0 // mazda 2019 BCM
#define MAZDA_AUX  1
#define MAZDA_CAM  2 // mazda 2019 not a camera

// CAN msgs we care about
#define MAZDA_LKAS          0x243
#define MAZDA_LKAS_HUD      0x440
#define MAZDA_CRZ_CTRL      0x21c
#define MAZDA_CRZ_BTNS      0x09d
#define MAZDA_STEER_TORQUE  0x240
#define MAZDA_ENGINE_DATA   0x202
#define MAZDA_PEDALS        0x165
const CanMsg MAZDA_TX_MSGS[] = {
  {MAZDA_LKAS, MAZDA_MAIN, 8}, 
  {MAZDA_CRZ_BTNS, MAZDA_MAIN, 8}, 
  {MAZDA_LKAS_HUD, MAZDA_MAIN, 8},
  };

/*
  Mazda Gen 4 2019+
  Mazda 3 2019+
  CX-30 to 90
*/ 
#define MAZDA_2019_BRAKE          0x43F // main bus
#define MAZDA_2019_GAS            0x202 // camera bus DBC: ENGINE_DATA
#define MAZDA_2019_CRUISE         0x44A // main bus. DBC: CRUISE_STATE
#define MAZDA_2019_SPEED          0x217 // camera bus. DBC: SPEED 
#define MAZDA_2019_STEER_TORQUE   0x24B // aux bus. DBC: EPS_FEEDBACK
#define MAZDA_2019_LKAS           0x249 // aux bus. DBC: EPS_LKAS
#define MAZDA_2019_CRZ_BTNS       0x9d  // rx on main tx on camera. DBC: CRZ_BTNS
#define MAZDA_2019_ACC            0x220 // main bus. DBC: ACC
const CanMsg MAZDA_2019_TX_MSGS[] = {
  {MAZDA_2019_LKAS, MAZDA_AUX, 8}, 
  {MAZDA_2019_ACC, MAZDA_CAM, 8},
  };

const LongitudinalLimits MAZDA_2019_LONG_LIMITS = {
  .max_accel = 3000,  // arbitrary limit
  .min_accel = 1000, // arbitrary limit
};

static bool mazda_2019 = false;

const SteeringLimits MAZDA_STEERING_LIMITS = {
  .max_steer = 800,
  .max_rate_up = 10,
  .max_rate_down = 25,
  .max_rt_delta = 300,
  .max_rt_interval = 250000,
  .driver_torque_factor = 1,
  .driver_torque_allowance = 15,
  .type = TorqueDriverLimited,
};

const SteeringLimits MAZDA_2019_STEERING_LIMITS = {
  .max_steer = 8000,
  .max_rate_up = 45,
  .max_rate_down = 80,
  .max_rt_delta = 3500,
  .max_rt_interval = 250000,
  .driver_torque_factor = 1,
  .driver_torque_allowance = 2000,
  .type = TorqueDriverLimited,
};

AddrCheckStruct mazda_addr_checks[] = {
  {.msg = {{MAZDA_CRZ_CTRL,     0, 8, .expected_timestep = 20000U}, { 0 }, { 0 }}},
  {.msg = {{MAZDA_CRZ_BTNS,     0, 8, .expected_timestep = 100000U}, { 0 }, { 0 }}},
  {.msg = {{MAZDA_STEER_TORQUE, 0, 8, .expected_timestep = 12000U}, { 0 }, { 0 }}},
  {.msg = {{MAZDA_ENGINE_DATA,  0, 8, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{MAZDA_PEDALS,       0, 8, .expected_timestep = 20000U}, { 0 }, { 0 }}},
};
#define MAZDA_ADDR_CHECKS_LEN (sizeof(mazda_addr_checks) / sizeof(mazda_addr_checks[0]))
addr_checks mazda_rx_checks = {mazda_addr_checks, MAZDA_ADDR_CHECKS_LEN};

AddrCheckStruct mazda_2019_addr_checks[] = {
  {.msg = {{MAZDA_2019_BRAKE,     0, 8, .expected_timestep = 1000000U}, { 0 }, { 0 }}},
  {.msg = {{MAZDA_2019_GAS,       2, 8, .expected_timestep = 1000000U}, { 0 }, { 0 }}},
  {.msg = {{MAZDA_2019_CRUISE,    0, 8, .expected_timestep = 1000000U}, { 0 }, { 0 }}},
  {.msg = {{MAZDA_2019_SPEED,     2, 8, .expected_timestep = 1000000U}, { 0 }, { 0 }}},
  {.msg = {{MAZDA_2019_STEER_TORQUE,     1, 8, .expected_timestep = 1000000U}, { 0 }, { 0 }}},
};

#define MAZDA_2019_ADDR_CHECKS_LEN (sizeof(mazda_2019_addr_checks) / sizeof(mazda_2019_addr_checks[0]))
addr_checks mazda_2019_rx_checks = {mazda_2019_addr_checks, MAZDA_2019_ADDR_CHECKS_LEN};

// track msgs coming from OP so that we know what CAM msgs to drop and what to forward
static int mazda_rx_hook(CANPacket_t *to_push) {
  bool valid = addr_safety_check(to_push, &mazda_rx_checks, NULL, NULL, NULL);
  if (valid) {
    int bus = GET_BUS(to_push);
    int addr = GET_ADDR(to_push);
    static bool cruise_engaged;
    static int speed;

    switch (bus) {
      case MAZDA_MAIN:
        if (mazda_2019) {
          switch (addr) {
            case MAZDA_2019_BRAKE:
              brake_pressed = (GET_BYTE(to_push, 5) & 0x4U);
              break; // end MAZDA_2019_BRAKE

            case MAZDA_2019_CRUISE:
              cruise_engaged = GET_BYTE(to_push, 0) & 0x20U;
              bool pre_enable = GET_BYTE(to_push, 0) & 0x40U;
              pcm_cruise_check((cruise_engaged || pre_enable));
              break; // end MAZDA_2019_CRUISE

            case MAZDA_2019_GAS: // should not be on this bus
              generic_rx_checks(true);
              break; // end MAZDA_2019_GAS

            default: // default addr
              break;
          } // end switch addr
        } else { // prev gen mazda
          switch (addr) {
            case MAZDA_ENGINE_DATA:
              // sample speed: scale by 0.01 to get kph
              speed = (GET_BYTE(to_push, 2) << 8) | GET_BYTE(to_push, 3);
              vehicle_moving = speed > 10; // moving when speed > 0.1 kph
              gas_pressed = (GET_BYTE(to_push, 4) || (GET_BYTE(to_push, 5) & 0xF0U));
              break; // end MAZDA_ENGINE_DATA addr
            
            case MAZDA_STEER_TORQUE:
              // update array of samples
              update_sample(&torque_driver, GET_BYTE(to_push, 0) - 127U);
              break; // end MAZDA_STEER_TORQUE addr

            case MAZDA_CRZ_CTRL:
              pcm_cruise_check((GET_BYTE(to_push, 0) & 0x20U));
              break; // end MAZDA_CRUISE addr
            
            case MAZDA_PEDALS:
              brake_pressed = (GET_BYTE(to_push, 0) & 0x10U);
              break; // end MAZDA_PEDALS addr
            
            case MAZDA_LKAS: // should not be on this bus
              generic_rx_checks(true);
              break; // end MAZDA_LKAS addr

            default: // default addr
              break;
          } // end switch addr
        } // end prev gen mazda
        break; // end MAZDA_MAIN bus

      case MAZDA_CAM:
        if (mazda_2019) {
          switch (addr) {
            case MAZDA_2019_GAS:
              gas_pressed = (GET_BYTE(to_push, 4) || (GET_BYTE(to_push, 5) & 0xC0U));
              break; // end MAZDA_2019_GAS addr

            case MAZDA_2019_SPEED:
              // sample speed: scale by 0.01 to get kph
              speed = (GET_BYTE(to_push, 4) << 8) | (GET_BYTE(to_push, 5));
              vehicle_moving = (speed > 10); // moving when speed > 0.1 kph
              break; // end MAZDA_2019_SPEED addr
            
            default: // default address cam
              break;
          } // end switch addr
        } // end mazda_2019
        break; // end MAZDA_CAM

      case MAZDA_AUX:
        if(mazda_2019){
          switch (addr) {
            case MAZDA_2019_STEER_TORQUE:
              update_sample(&torque_driver, GET_BYTE(to_push, 0) << 8 | GET_BYTE(to_push, 1));
              break; // end TI2_STEER_TORQUE
            
            default: // default address aux
              break;
          }
        }
        break; // end MAZDA_AUX 

      default: // default bus
        break;
    }
  }
  return valid;
}

static int mazda_tx_hook(CANPacket_t *to_send, bool longitudinal_allowed) {
  int tx = 1;
  int addr = GET_ADDR(to_send);
  int bus = GET_BUS(to_send);

  if (mazda_2019) {
    if (!msg_allowed(to_send, MAZDA_2019_TX_MSGS, sizeof(MAZDA_2019_TX_MSGS)/sizeof(MAZDA_2019_TX_MSGS[0]))) {
      tx = 0;
    }
    if (bus == MAZDA_AUX) {
      if (addr == MAZDA_2019_LKAS) {
        int desired_torque = ((GET_BYTE(to_send, 0) << 8) | GET_BYTE(to_send, 1)); // signal is signed
        if (steer_torque_cmd_checks(desired_torque, -1, MAZDA_2019_STEERING_LIMITS)) {
          //tx = 0;
        }
      }
      if (addr == MAZDA_2019_ACC) {
        int desired_accel = ( (GET_BYTE(to_send, 2) & 0x01U << 11) | (GET_BYTE(to_send, 3) << 3) | ( GET_BYTE(to_send, 3) & 0xE0U) ); // signal is signed
        if (longitudinal_allowed) {
          if (longitudinal_accel_checks(desired_accel, MAZDA_2019_LONG_LIMITS, longitudinal_allowed)) {
            //tx = 0;
          }
        }
      }
    }
  } else { // prev gen mazda
    if (!msg_allowed(to_send, MAZDA_TX_MSGS, sizeof(MAZDA_TX_MSGS)/sizeof(MAZDA_TX_MSGS[0]))) {
      tx = 0;
    }
    // Check if msg is sent on the main BUS
    if (bus == MAZDA_MAIN) {
      
      // steer cmd checks
      if (addr == MAZDA_LKAS) {
        int desired_torque = (((GET_BYTE(to_send, 0) & 0x0FU) << 8) | GET_BYTE(to_send, 1)) - 2048U;

        if (steer_torque_cmd_checks(desired_torque, -1, MAZDA_STEERING_LIMITS)) {
          tx = 0;
        }
      }

      // cruise buttons check
      if (addr == MAZDA_CRZ_BTNS) {
        // allow resume spamming while controls allowed, but
        // only allow cancel while contrls not allowed
        bool cancel_cmd = (GET_BYTE(to_send, 0) == 0x1U);
        if (!controls_allowed && !cancel_cmd) {
          tx = 0;
        }
      }
    }
  }

  return tx;
}

static int mazda_fwd_hook(int bus, CANPacket_t *to_fwd) {
  int bus_fwd = -1;
  int addr = GET_ADDR(to_fwd);
  bool block = false;
  switch (bus){
    case MAZDA_MAIN:
      if (mazda_2019){
        block = (addr == MAZDA_2019_ACC);
      }
      if (!block) {
      bus_fwd = MAZDA_CAM;
      }
      break; // end MAZDA_MAIN
    case MAZDA_CAM:
      if (mazda_2019){
        // don't block
      } else {
        block = (addr == MAZDA_LKAS) || (addr == MAZDA_LKAS_HUD);
      }
      if (!block) {
        bus_fwd = MAZDA_MAIN;
      }
      break; // end MAZDA_CAM

    default: // default bus
      // don't forward
      break;
  }
  return bus_fwd;
}

static const addr_checks* mazda_init(uint16_t param) {
  param;
  if (param == 1U){
    mazda_rx_checks = (addr_checks){mazda_2019_addr_checks, MAZDA_2019_ADDR_CHECKS_LEN};
    mazda_2019 = true;
  }
  return &mazda_rx_checks;
}

const safety_hooks mazda_hooks = {
  .init = mazda_init,
  .rx = mazda_rx_hook,
  .tx = mazda_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = mazda_fwd_hook,
};