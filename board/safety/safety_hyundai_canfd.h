#include "safety_hyundai_common.h"

const SteeringLimits HYUNDAI_CANFD_STEERING_LIMITS = {
  .max_steer = 270,
  .max_rt_delta = 112,
  .max_rt_interval = 250000,
  .max_rate_up = 2,
  .max_rate_down = 3,
  .driver_torque_allowance = 250,
  .driver_torque_factor = 2,
  .type = TorqueDriverLimited,

  // the EPS faults when the steering angle is above a certain threshold for too long. to prevent this,
  // we allow setting torque actuation bit to 0 while maintaining the requested torque value for two consecutive frames
  .min_valid_request_frames = 89,
  .max_invalid_request_frames = 2,
  .min_valid_request_rt_interval = 810000,  // 810ms; a ~10% buffer on cutting every 90 frames
  .has_steer_req_tolerance = true,
};

#define HYUNDAI_CANFD_LKAS                      0x50
#define HYUNDAI_CANFD_CRUISE_BUTTONS            0x1CF
#define HYUNDAI_CANFD_CAM_0x2A4                 0x2A4
#define HYUNDAI_CANFD_ADRV_0x51                 0x51
#define HYUNDAI_CANFD_ADAS_TESTER_PRESENT       0x730
#define HYUNDAI_CANFD_LFA                       0x12A
#define HYUNDAI_CANFD_ADRV_0x160                0x160
#define HYUNDAI_CANFD_LFAHDA_CLUSTER            0x1E0
#define HYUNDAI_CANFD_SCC_CONTROL               0x1A0
#define HYUNDAI_CANFD_ADRV_0x1EA                0x1EA
#define HYUNDAI_CANFD_ADRV_0x200                0x200
#define HYUNDAI_CANFD_ADRV_0x345                0x345
#define HYUNDAI_CANFD_ADRV_0X1DA                0x1DA

#define HYUNDAI_CANFD_ACCELERATOR               0x35
#define HYUNDAI_CANFD_ACCELERATOR_ALT           0x105
#define HYUNDAI_CANFD_TCS                       0x175
#define HYUNDAI_CANFD_WHEEL_SPEEDS              0xA0
#define HYUNDAI_CANFD_MDPS                      0xEA
#define HYUNDAI_CANFD_CRUISE_BUTTONS_ALT        0x1AA
#define HYUNDAI_CANFD_ACCELERATOR_BRAKE_ALT     0x100

const CanMsg HYUNDAI_CANFD_HDA2_TX_MSGS[] = {
  {HYUNDAI_CANFD_LKAS,                HYUNDAI_MAIN_BUS, 16},
  {HYUNDAI_CANFD_CRUISE_BUTTONS,      HYUNDAI_SUB_BUS,   8},
  {HYUNDAI_CANFD_CAM_0x2A4,           HYUNDAI_MAIN_BUS, 24},
};

const CanMsg HYUNDAI_CANFD_HDA2_LONG_TX_MSGS[] = {
  {HYUNDAI_CANFD_LKAS,                HYUNDAI_MAIN_BUS, 16},
  {HYUNDAI_CANFD_CRUISE_BUTTONS,      HYUNDAI_SUB_BUS,   8},
  {HYUNDAI_CANFD_CAM_0x2A4,           HYUNDAI_MAIN_BUS, 24},
  {HYUNDAI_CANFD_ADRV_0x51,           HYUNDAI_MAIN_BUS, 32},
  {HYUNDAI_CANFD_ADAS_TESTER_PRESENT, HYUNDAI_SUB_BUS,   8},  // tester present for ADAS ECU disable
  {HYUNDAI_CANFD_LFA,                 HYUNDAI_SUB_BUS,  16},
  {HYUNDAI_CANFD_ADRV_0x160,          HYUNDAI_SUB_BUS,  16},
  {HYUNDAI_CANFD_LFAHDA_CLUSTER,      HYUNDAI_SUB_BUS,  16},
  {HYUNDAI_CANFD_SCC_CONTROL,         HYUNDAI_SUB_BUS,  32},
  {HYUNDAI_CANFD_ADRV_0x1EA,          HYUNDAI_SUB_BUS,  32},
  {HYUNDAI_CANFD_ADRV_0x200,          HYUNDAI_SUB_BUS,   8},
  {HYUNDAI_CANFD_ADRV_0x345,          HYUNDAI_SUB_BUS,   8},
  {HYUNDAI_CANFD_ADRV_0X1DA,          HYUNDAI_SUB_BUS,  32},
};

const CanMsg HYUNDAI_CANFD_HDA1_TX_MSGS[] = {
  {HYUNDAI_CANFD_LFA,                 HYUNDAI_MAIN_BUS, 16},
  {HYUNDAI_CANFD_SCC_CONTROL,         HYUNDAI_MAIN_BUS, 32},
  {HYUNDAI_CANFD_CRUISE_BUTTONS,      HYUNDAI_MAIN_BUS,  8},
  {HYUNDAI_CANFD_LFAHDA_CLUSTER,      HYUNDAI_MAIN_BUS, 16},
};

AddrCheckStruct hyundai_canfd_addr_checks[] = {
  {.msg = {{HYUNDAI_CANFD_ACCELERATOR,        HYUNDAI_SUB_BUS,  32, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U},
           {HYUNDAI_CANFD_ACCELERATOR,        HYUNDAI_MAIN_BUS, 32, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U},
           {HYUNDAI_CANFD_ACCELERATOR_ALT,    HYUNDAI_MAIN_BUS, 32, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U}}},
  {.msg = {{HYUNDAI_CANFD_TCS,                HYUNDAI_SUB_BUS,  24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 20000U},
           {HYUNDAI_CANFD_TCS,                HYUNDAI_MAIN_BUS, 24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 20000U}, { 0 }}},
  {.msg = {{HYUNDAI_CANFD_WHEEL_SPEEDS,       HYUNDAI_SUB_BUS,  24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U},
           {HYUNDAI_CANFD_WHEEL_SPEEDS,       HYUNDAI_MAIN_BUS, 24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U}, { 0 }}},
  {.msg = {{HYUNDAI_CANFD_MDPS,               HYUNDAI_SUB_BUS,  24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U},
           {HYUNDAI_CANFD_MDPS,               HYUNDAI_MAIN_BUS, 24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U}, { 0 }}},
  {.msg = {{HYUNDAI_CANFD_SCC_CONTROL,        HYUNDAI_SUB_BUS,  32, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 20000U},
           {HYUNDAI_CANFD_SCC_CONTROL,        HYUNDAI_CAM_BUS,  32, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 20000U}, { 0 }}},
  {.msg = {{HYUNDAI_CANFD_CRUISE_BUTTONS,     HYUNDAI_SUB_BUS,   8, .check_checksum = false, .max_counter = 0xfU, .expected_timestep = 20000U},
           {HYUNDAI_CANFD_CRUISE_BUTTONS,     HYUNDAI_MAIN_BUS,  8, .check_checksum = false, .max_counter = 0xfU, .expected_timestep = 20000U},
           {HYUNDAI_CANFD_CRUISE_BUTTONS_ALT, HYUNDAI_MAIN_BUS, 16, .check_checksum = false, .max_counter = 0xffU, .expected_timestep = 20000U}}},
};
#define HYUNDAI_CANFD_ADDR_CHECK_LEN (sizeof(hyundai_canfd_addr_checks) / sizeof(hyundai_canfd_addr_checks[0]))

// HYUNDAI_CANFD_SCC_CONTROL is on bus 0
AddrCheckStruct hyundai_canfd_radar_scc_addr_checks[] = {
  {.msg = {{HYUNDAI_CANFD_ACCELERATOR,        HYUNDAI_SUB_BUS,  32, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U},
           {HYUNDAI_CANFD_ACCELERATOR,        HYUNDAI_MAIN_BUS, 32, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U},
           {HYUNDAI_CANFD_ACCELERATOR_ALT,    HYUNDAI_MAIN_BUS, 32, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U}}},
  {.msg = {{HYUNDAI_CANFD_TCS,                HYUNDAI_SUB_BUS,  24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 20000U},
           {HYUNDAI_CANFD_TCS,                HYUNDAI_MAIN_BUS, 24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 20000U}, { 0 }}},
  {.msg = {{HYUNDAI_CANFD_WHEEL_SPEEDS,       HYUNDAI_SUB_BUS,  24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U},
           {HYUNDAI_CANFD_WHEEL_SPEEDS,       HYUNDAI_MAIN_BUS, 24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U}, { 0 }}},
  {.msg = {{HYUNDAI_CANFD_MDPS,               HYUNDAI_SUB_BUS,  24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U},
           {HYUNDAI_CANFD_MDPS,               HYUNDAI_MAIN_BUS, 24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U}, { 0 }}},
  {.msg = {{HYUNDAI_CANFD_SCC_CONTROL,        HYUNDAI_MAIN_BUS, 32, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 20000U}, { 0 }, { 0 }}},
  {.msg = {{HYUNDAI_CANFD_CRUISE_BUTTONS,     HYUNDAI_SUB_BUS,   8, .check_checksum = false, .max_counter = 0xfU, .expected_timestep = 20000U},
           {HYUNDAI_CANFD_CRUISE_BUTTONS,     HYUNDAI_MAIN_BUS,  8, .check_checksum = false, .max_counter = 0xfU, .expected_timestep = 20000U},
           {HYUNDAI_CANFD_CRUISE_BUTTONS_ALT, HYUNDAI_MAIN_BUS, 16, .check_checksum = false, .max_counter = 0xffU, .expected_timestep = 20000U}}},
};
#define HYUNDAI_CANFD_RADAR_SCC_ADDR_CHECK_LEN (sizeof(hyundai_canfd_radar_scc_addr_checks) / sizeof(hyundai_canfd_radar_scc_addr_checks[0]))

AddrCheckStruct hyundai_canfd_long_addr_checks[] = {
  {.msg = {{HYUNDAI_CANFD_ACCELERATOR,        HYUNDAI_SUB_BUS,  32, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U},
           {HYUNDAI_CANFD_ACCELERATOR,        HYUNDAI_MAIN_BUS, 32, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U},
           {HYUNDAI_CANFD_ACCELERATOR_ALT,    HYUNDAI_MAIN_BUS, 32, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U}}},
  {.msg = {{HYUNDAI_CANFD_TCS,                HYUNDAI_SUB_BUS,  24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 20000U},
           {HYUNDAI_CANFD_TCS,                HYUNDAI_MAIN_BUS, 24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 20000U}, { 0 }}},
  {.msg = {{HYUNDAI_CANFD_WHEEL_SPEEDS,       HYUNDAI_SUB_BUS,  24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U},
           {HYUNDAI_CANFD_WHEEL_SPEEDS,       HYUNDAI_MAIN_BUS, 24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U}, { 0 }}},
  {.msg = {{HYUNDAI_CANFD_MDPS,               HYUNDAI_SUB_BUS,  24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U},
           {HYUNDAI_CANFD_MDPS,               HYUNDAI_MAIN_BUS, 24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U}, { 0 }}},
  {.msg = {{HYUNDAI_CANFD_CRUISE_BUTTONS,     HYUNDAI_SUB_BUS,   8, .check_checksum = false, .max_counter = 0xfU, .expected_timestep = 20000U},
           {HYUNDAI_CANFD_CRUISE_BUTTONS,     HYUNDAI_MAIN_BUS,  8, .check_checksum = false, .max_counter = 0xfU, .expected_timestep = 20000U},
           {HYUNDAI_CANFD_CRUISE_BUTTONS_ALT, HYUNDAI_MAIN_BUS, 16, .check_checksum = false, .max_counter = 0xffU, .expected_timestep = 20000U}}},
};
#define HYUNDAI_CANFD_LONG_ADDR_CHECK_LEN (sizeof(hyundai_canfd_long_addr_checks) / sizeof(hyundai_canfd_long_addr_checks[0]))

AddrCheckStruct hyundai_canfd_ice_addr_checks[] = {
  {.msg = {{HYUNDAI_CANFD_ACCELERATOR_BRAKE_ALT, HYUNDAI_MAIN_BUS, 32, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{HYUNDAI_CANFD_WHEEL_SPEEDS,          HYUNDAI_MAIN_BUS, 24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{HYUNDAI_CANFD_MDPS,                  HYUNDAI_MAIN_BUS, 24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 10000U}, { 0 }, { 0 }}},
  {.msg = {{HYUNDAI_CANFD_TCS,                   HYUNDAI_MAIN_BUS, 24, .check_checksum = true, .max_counter = 0xffU, .expected_timestep = 20000U}, { 0 }, { 0 }}},
  {.msg = {{HYUNDAI_CANFD_CRUISE_BUTTONS_ALT,    HYUNDAI_MAIN_BUS, 16, .check_checksum = false, .max_counter = 0xffU, .expected_timestep = 20000U}, { 0 }, { 0 }}},
};
#define HYUNDAI_CANFD_ICE_ADDR_CHECK_LEN (sizeof(hyundai_canfd_ice_addr_checks) / sizeof(hyundai_canfd_ice_addr_checks[0]))

addr_checks hyundai_canfd_rx_checks = {hyundai_canfd_addr_checks, HYUNDAI_CANFD_ADDR_CHECK_LEN};


uint16_t hyundai_canfd_crc_lut[256];


const int HYUNDAI_PARAM_CANFD_HDA2 = 16;
const int HYUNDAI_PARAM_CANFD_ALT_BUTTONS = 32;
bool hyundai_canfd_hda2 = false;
bool hyundai_canfd_alt_buttons = false;


static uint8_t hyundai_canfd_get_counter(CANPacket_t *to_push) {
  uint8_t ret = 0;
  if (GET_LEN(to_push) == 8U) {
    ret = GET_BYTE(to_push, 1) >> 4;
  } else {
    ret = GET_BYTE(to_push, 2);
  }
  return ret;
}

static uint32_t hyundai_canfd_get_checksum(CANPacket_t *to_push) {
  uint32_t chksum = GET_BYTE(to_push, 0) | (GET_BYTE(to_push, 1) << 8);
  return chksum;
}

static uint32_t hyundai_canfd_compute_checksum(CANPacket_t *to_push) {
  int len = GET_LEN(to_push);
  uint32_t address = GET_ADDR(to_push);

  uint16_t crc = 0;

  for (int i = 2; i < len; i++) {
    crc = (crc << 8U) ^ hyundai_canfd_crc_lut[(crc >> 8U) ^ GET_BYTE(to_push, i)];
  }

  // Add address to crc
  crc = (crc << 8U) ^ hyundai_canfd_crc_lut[(crc >> 8U) ^ ((address >> 0U) & 0xFFU)];
  crc = (crc << 8U) ^ hyundai_canfd_crc_lut[(crc >> 8U) ^ ((address >> 8U) & 0xFFU)];

  if (len == 8) {
    crc ^= 0x5f29U;
  } else if (len == 16) {
    crc ^= 0x041dU;
  } else if (len == 24) {
    crc ^= 0x819dU;
  } else if (len == 32) {
    crc ^= 0x9f5bU;
  } else {

  }

  return crc;
}

static int hyundai_canfd_rx_hook(CANPacket_t *to_push) {

  bool valid = addr_safety_check(to_push, &hyundai_canfd_rx_checks,
                                 hyundai_canfd_get_checksum, hyundai_canfd_compute_checksum, hyundai_canfd_get_counter, NULL);

  int bus = GET_BUS(to_push);
  int addr = GET_ADDR(to_push);

  const int pt_bus = hyundai_canfd_hda2 ? HYUNDAI_SUB_BUS : HYUNDAI_MAIN_BUS;
  const int scc_bus = hyundai_camera_scc ? HYUNDAI_CAM_BUS : pt_bus;

  if (valid && (bus == pt_bus)) {
    // driver torque
    if (addr == HYUNDAI_CANFD_MDPS) {
      int torque_driver_new = ((GET_BYTE(to_push, 11) & 0x1fU) << 8U) | GET_BYTE(to_push, 10);
      torque_driver_new -= 4095;
      update_sample(&torque_driver, torque_driver_new);
    }

    // cruise buttons
    const int button_addr = hyundai_canfd_alt_buttons ? HYUNDAI_CANFD_CRUISE_BUTTONS_ALT : HYUNDAI_CANFD_CRUISE_BUTTONS;
    if (addr == button_addr) {
      int main_button = 0;
      int cruise_button = 0;
      if (addr == HYUNDAI_CANFD_CRUISE_BUTTONS) {
        cruise_button = GET_BYTE(to_push, 2) & 0x7U;
        main_button = GET_BIT(to_push, 19U);
      } else {
        cruise_button = (GET_BYTE(to_push, 4) >> 4) & 0x7U;
        main_button = GET_BIT(to_push, 34U);
      }
      hyundai_common_cruise_buttons_check(cruise_button, main_button);
    }

    // gas press, different for EV, hybrid, and ICE models
    if ((addr == HYUNDAI_CANFD_ACCELERATOR) && hyundai_ev_gas_signal) {
      gas_pressed = GET_BYTE(to_push, 5) != 0U;
    } else if ((addr == HYUNDAI_CANFD_ACCELERATOR_ALT) && hyundai_hybrid_gas_signal) {
      gas_pressed = (GET_BIT(to_push, 103U) != 0U) || (GET_BYTE(to_push, 13) != 0U) || (GET_BIT(to_push, 112U) != 0U);
    } else if ((addr == HYUNDAI_CANFD_ACCELERATOR_BRAKE_ALT) && !hyundai_ev_gas_signal && !hyundai_hybrid_gas_signal) {
      gas_pressed = GET_BIT(to_push, 176U) != 0U;
    } else {
    }

    // brake press
    if (addr == HYUNDAI_CANFD_TCS) {
      brake_pressed = GET_BIT(to_push, 81U) != 0U;
    }

    // vehicle moving
    if (addr == HYUNDAI_CANFD_WHEEL_SPEEDS) {
      uint32_t speed = 0;
      for (int i = 8; i < 15; i+=2) {
        speed += GET_BYTE(to_push, i) | (GET_BYTE(to_push, i + 1) << 8U);
      }
      vehicle_moving = (speed / 4U) > HYUNDAI_STANDSTILL_THRSLD;
    }
  }

  if (valid && (bus == scc_bus)) {
    // cruise state
    if ((addr == HYUNDAI_CANFD_SCC_CONTROL) && !hyundai_longitudinal) {
      bool cruise_engaged = ((GET_BYTE(to_push, 8) >> 4) & 0x3U) != 0U;
      hyundai_common_cruise_state_check(cruise_engaged);
    }
  }

  const int steer_addr = hyundai_canfd_hda2 ? HYUNDAI_CANFD_LKAS : HYUNDAI_CANFD_LFA;
  bool stock_ecu_detected = (addr == steer_addr) && (bus == HYUNDAI_MAIN_BUS);
  if (hyundai_longitudinal) {
    // on HDA2, ensure ADRV ECU is still knocked out
    // on others, ensure accel msg is blocked from camera
    const int stock_scc_bus = hyundai_canfd_hda2 ? HYUNDAI_SUB_BUS : HYUNDAI_MAIN_BUS;
    stock_ecu_detected = stock_ecu_detected || ((addr == HYUNDAI_CANFD_SCC_CONTROL) && (bus == stock_scc_bus));
  }
  generic_rx_checks(stock_ecu_detected);

  return valid;
}

static int hyundai_canfd_tx_hook(CANPacket_t *to_send) {

  int tx = 0;
  int addr = GET_ADDR(to_send);

  if (hyundai_canfd_hda2 && !hyundai_longitudinal) {
    tx = msg_allowed(to_send, HYUNDAI_CANFD_HDA2_TX_MSGS, sizeof(HYUNDAI_CANFD_HDA2_TX_MSGS)/sizeof(HYUNDAI_CANFD_HDA2_TX_MSGS[0]));
  } else if (hyundai_canfd_hda2 && hyundai_longitudinal) {
    tx = msg_allowed(to_send, HYUNDAI_CANFD_HDA2_LONG_TX_MSGS, sizeof(HYUNDAI_CANFD_HDA2_LONG_TX_MSGS)/sizeof(HYUNDAI_CANFD_HDA2_LONG_TX_MSGS[0]));
  } else {
    tx = msg_allowed(to_send, HYUNDAI_CANFD_HDA1_TX_MSGS, sizeof(HYUNDAI_CANFD_HDA1_TX_MSGS)/sizeof(HYUNDAI_CANFD_HDA1_TX_MSGS[0]));
  }

  // steering
  const int steer_addr = (hyundai_canfd_hda2 && !hyundai_longitudinal) ? HYUNDAI_CANFD_LKAS : HYUNDAI_CANFD_LFA;
  if (addr == steer_addr) {
    int desired_torque = (((GET_BYTE(to_send, 6) & 0xFU) << 7U) | (GET_BYTE(to_send, 5) >> 1U)) - 1024U;
    bool steer_req = GET_BIT(to_send, 52U) != 0U;

    if (steer_torque_cmd_checks(desired_torque, steer_req, HYUNDAI_CANFD_STEERING_LIMITS)) {
      tx = 0;
    }
  }

  // cruise buttons check
  if (addr == HYUNDAI_CANFD_CRUISE_BUTTONS) {
    int button = GET_BYTE(to_send, 2) & 0x7U;
    bool is_cancel = (button == HYUNDAI_BTN_CANCEL);
    bool is_resume = (button == HYUNDAI_BTN_RESUME);

    bool allowed = (is_cancel && cruise_engaged_prev) || (is_resume && controls_allowed);
    if (!allowed) {
      tx = 0;
    }
  }

  // UDS: only tester present ("\x02\x3E\x80\x00\x00\x00\x00\x00") allowed on diagnostics address
  if ((addr == HYUNDAI_CANFD_ADAS_TESTER_PRESENT) && hyundai_canfd_hda2) {
    if ((GET_BYTES(to_send, 0, 4) != 0x00803E02U) || (GET_BYTES(to_send, 4, 4) != 0x0U)) {
      tx = 0;
    }
  }

  // ACCEL: safety check
  if (addr == HYUNDAI_CANFD_SCC_CONTROL) {
    int desired_accel_raw = (((GET_BYTE(to_send, 17) & 0x7U) << 8) | GET_BYTE(to_send, 16)) - 1023U;
    int desired_accel_val = ((GET_BYTE(to_send, 18) << 4) | (GET_BYTE(to_send, 17) >> 4)) - 1023U;

    bool violation = false;

    if (hyundai_longitudinal) {
      violation |= longitudinal_accel_checks(desired_accel_raw, HYUNDAI_LONG_LIMITS);
      violation |= longitudinal_accel_checks(desired_accel_val, HYUNDAI_LONG_LIMITS);
    } else {
      // only used to cancel on here
      if ((desired_accel_raw != 0) || (desired_accel_val != 0)) {
        violation = true;
      }
    }

    if (violation) {
      tx = 0;
    }
  }

  return tx;
}

static int hyundai_canfd_fwd_hook(int bus_num, int addr) {
  int bus_fwd = -1;

  if (bus_num == HYUNDAI_MAIN_BUS) {
    bus_fwd = HYUNDAI_CAM_BUS;
  }
  if (bus_num == HYUNDAI_CAM_BUS) {
    // LKAS for HDA2, LFA for HDA1
    int is_lkas_msg = (((addr == HYUNDAI_CANFD_LKAS) || (addr == HYUNDAI_CANFD_CAM_0x2A4)) && hyundai_canfd_hda2);
    int is_lfa_msg = ((addr == HYUNDAI_CANFD_LFA) && !hyundai_canfd_hda2);

    // HUD icons
    int is_lfahda_msg = ((addr == HYUNDAI_CANFD_LFAHDA_CLUSTER) && !hyundai_canfd_hda2);

    // CRUISE_INFO for non-HDA2, we send our own longitudinal commands
    int is_scc_msg = ((addr == HYUNDAI_CANFD_SCC_CONTROL) && hyundai_longitudinal && !hyundai_canfd_hda2);

    int block_msg = is_lkas_msg || is_lfa_msg || is_lfahda_msg || is_scc_msg;
    if (!block_msg) {
      bus_fwd = HYUNDAI_MAIN_BUS;
    }
  }

  return bus_fwd;
}

static const addr_checks* hyundai_canfd_init(uint16_t param) {
  hyundai_common_init(param);

  gen_crc_lookup_table_16(0x1021, hyundai_canfd_crc_lut);
  hyundai_canfd_hda2 = GET_FLAG(param, HYUNDAI_PARAM_CANFD_HDA2);
  hyundai_canfd_alt_buttons = GET_FLAG(param, HYUNDAI_PARAM_CANFD_ALT_BUTTONS);

  // no long for ICE yet
  if (!hyundai_ev_gas_signal && !hyundai_hybrid_gas_signal) {
    hyundai_longitudinal = false;
  }

  if (hyundai_longitudinal) {
    hyundai_canfd_rx_checks = (addr_checks){hyundai_canfd_long_addr_checks, HYUNDAI_CANFD_LONG_ADDR_CHECK_LEN};
  } else {
    if (!hyundai_ev_gas_signal && !hyundai_hybrid_gas_signal) {
      hyundai_canfd_rx_checks = (addr_checks){hyundai_canfd_ice_addr_checks, HYUNDAI_CANFD_ICE_ADDR_CHECK_LEN};
    } else if (!hyundai_camera_scc && !hyundai_canfd_hda2) {
      hyundai_canfd_rx_checks = (addr_checks){hyundai_canfd_radar_scc_addr_checks, HYUNDAI_CANFD_RADAR_SCC_ADDR_CHECK_LEN};
    } else {
      hyundai_canfd_rx_checks = (addr_checks){hyundai_canfd_addr_checks, HYUNDAI_CANFD_ADDR_CHECK_LEN};
    }
  }

  return &hyundai_canfd_rx_checks;
}

const safety_hooks hyundai_canfd_hooks = {
  .init = hyundai_canfd_init,
  .rx = hyundai_canfd_rx_hook,
  .tx = hyundai_canfd_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = hyundai_canfd_fwd_hook,
};
