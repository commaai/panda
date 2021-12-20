const struct lookup_t FORD_LOOKUP_ANGLE_RATE_UP = {
  {2., 7., 17.},
  {5., .8, .25}};

const struct lookup_t FORD_LOOKUP_ANGLE_RATE_DOWN = {
  {2., 7., 17.},
  {5., 3.5, .8}};

// Signal: LatCtlPath_An_Actl
// Factor: 0.0005
const int FORD_DEG_TO_CAN = 2000;

// Safety-relevant CAN messages for Ford vehicles.
#define MSG_STEERING_PINION_DATA      0x07E  // RX from PSCM, for steering pinion angle
#define MSG_ENG_BRAKE_DATA            0x165  // RX from PCM, for driver brake pedal and cruise state
#define MSG_ENG_VEHICLE_SP_THROTTLE2  0x202  // RX from PCM, for vehicle speed
#define MSG_ENG_VEHICLE_SP_THROTTLE   0x204  // RX from PCM, for driver throttle input
#define MSG_STEERING_DATA_FD1         0x083  // TX by OP, ACC control buttons for cancel
#define MSG_LANE_ASSIST_DATA1         0x3CA  // TX by OP, Lane Keeping Assist
#define MSG_LATERAL_MOTION_CONTROL    0x3D3  // TX by OP, Lane Centering Assist
#define MSG_IPMA_DATA                 0x3D8  // TX by OP, IPMA HUD user interface

// CAN bus numbers.
#define FORD_MAIN 0U
#define FORD_CAM  2U

const CanMsg FORD_TX_MSGS[] = {
  {MSG_STEERING_DATA_FD1, 0, 8},
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

static int ford_rx_hook(CANPacket_t *to_push) {

  int addr = GET_ADDR(to_push);
  int bus = GET_BUS(to_push);
  bool alt_exp_allow_gas = alternative_experience & ALT_EXP_DISABLE_DISENGAGE_ON_GAS;

  if (addr == 0x217) {
    // wheel speeds are 14 bits every 16
    vehicle_moving = false;
    for (int i = 0; i < 8; i += 2) {
      vehicle_moving |= GET_BYTE(to_push, i) | (GET_BYTE(to_push, (int)(i + 1)) & 0xFCU);
    }
  }

  // state machine to enter and exit controls
  if (addr == 0x83) {
    bool cancel = GET_BYTE(to_push, 1) & 0x1U;
    bool set_or_resume = GET_BYTE(to_push, 3) & 0x30U;
    if (cancel) {
      controls_allowed = 0;
    }
    if (set_or_resume) {
      controls_allowed = 1;
    }
  }

  // exit controls on rising edge of brake press or on brake press when
  // speed > 0
  if (addr == 0x165) {
    brake_pressed = GET_BYTE(to_push, 0) & 0x20U;
    if (brake_pressed && (!brake_pressed_prev || vehicle_moving)) {
      controls_allowed = 0;
    }
    brake_pressed_prev = brake_pressed;
  }

  // exit controls on rising edge of gas press
  if (addr == 0x204) {
    gas_pressed = ((GET_BYTE(to_push, 0) & 0x03U) | GET_BYTE(to_push, 1)) != 0U;
    if (!alt_exp_allow_gas && gas_pressed && !gas_pressed_prev) {
      controls_allowed = 0;
    }
    gas_pressed_prev = gas_pressed;
  }

  if ((safety_mode_cnt > RELAY_TRNS_TIMEOUT) && (bus == 0) && (addr == 0x3CA)) {
    relay_malfunction_set();
  }
  return 1;
}

// all commands: just steering
// if controls_allowed and no pedals pressed
//     allow all commands up to limit
// else
//     block all commands that produce actuation

static int ford_tx_hook(CANPacket_t *to_send, bool longitudinal_allowed) {
  UNUSED(longitudinal_allowed);

  int tx = 1;
  int addr = GET_ADDR(to_send);

  // disallow actuator commands if gas or brake (with vehicle moving) are pressed
  // and the the latching controls_allowed flag is True
  int pedal_pressed = brake_pressed_prev && vehicle_moving;
  bool alt_exp_allow_gas = alternative_experience & ALT_EXP_DISABLE_DISENGAGE_ON_GAS;
  if (!alt_exp_allow_gas) {
    pedal_pressed = pedal_pressed || gas_pressed_prev;
  }
  bool current_controls_allowed = controls_allowed && !(pedal_pressed);

  // STEER: safety check
  if (addr == 0x3CA) {
    if (!current_controls_allowed) {
      // bits 7-4 need to be 0xF to disallow lkas commands
      if ((GET_BYTE(to_send, 0) & 0xF0U) != 0xF0U) {
        tx = 0;
      }
    }
  }

  // FORCE CANCEL: safety check only relevant when spamming the cancel button
  // ensuring that set and resume aren't sent
  if (addr == 0x83) {
    if ((GET_BYTE(to_send, 3) & 0x30U) != 0U) {
      tx = 0;
    }
  }

  // 1 allows the message through
  return tx;
}

// TODO: keep camera on bus 2 and make a fwd_hook

const safety_hooks ford_hooks = {
  .init = nooutput_init,
  .rx = ford_rx_hook,
  .tx = ford_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .fwd = default_fwd_hook,
};
