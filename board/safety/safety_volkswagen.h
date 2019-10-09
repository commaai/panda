const int VW_MAX_STEER = 300;               // 3.0 nm
const int VW_MAX_RT_DELTA = 188;            // 10 max rate * 50Hz send rate * 250000 RT interval / 1000000 = 125 ; 125 * 1.5 for safety pad = 187.5
const uint32_t VW_RT_INTERVAL = 250000;     // 250ms between real time checks
const int VW_MAX_RATE_UP = 10;              // 5.0 nm/s available rate of change from the steering rack
const int VW_MAX_RATE_DOWN = 10;            // 5.0 nm/s available rate of change from the steering rack
const int VW_DRIVER_TORQUE_ALLOWANCE = 100;
const int VW_DRIVER_TORQUE_FACTOR = 4;

int vw_ignition_started = 0;
struct sample_t vw_torque_driver;           // last few driver torques measured
int vw_rt_torque_last = 0;
int vw_desired_torque_last = 0;
uint32_t vw_ts_last = 0;

// Safety-relevant CAN messages for the Volkswagen MQB platform.
#define MSG_EPS_01              0x09F
#define MSG_ACC_06              0x122
#define MSG_HCA_01              0x126
#define MSG_GRA_ACC_01          0x12B
#define MSG_LDW_02              0x397
#define MSG_KLEMMEN_STATUS_01   0x3C0

static void volkswagen_init(int16_t param) {
  UNUSED(param); // May use param in the future to indicate MQB vs PQ35/PQ46/NMS vs MLB, or wiring configuration.
  controls_allowed = 0;
  vw_ignition_started = 0;
}
static void volkswagen_rx_hook(CAN_FIFOMailBox_TypeDef *to_push) {
  int bus = GET_BUS(to_push);
  int addr = GET_ADDR(to_push);

  // Monitor Klemmen_Status_01.ZAS_Kl_15 for Terminal 15 (ignition-on) status, but we make no use of it at the moment.
  if ((bus == 0) && (addr == MSG_KLEMMEN_STATUS_01)) {
    vw_ignition_started = (GET_BYTE(to_push, 2) & 0x2) >> 1;
  }

  // Update driver input torque samples from EPS_01.Driver_Strain for absolute torque, and EPS_01.Driver_Strain_VZ
  // for the direction.
  if ((bus == 0) && (addr == MSG_EPS_01)) {
    int torque_driver_new = GET_BYTE(to_push, 5) | ((GET_BYTE(to_push, 6) & 0x1F) << 8);
    int sign = (GET_BYTE(to_push, 6) & 0x80) >> 7;
    if (sign == 1) {
      torque_driver_new *= -1;
    }

    update_sample(&vw_torque_driver, torque_driver_new);
  }

  // Monitor ACC_06.ACC_Status_ACC for stock ACC status. Because the current MQB port is lateral-only, OP's control
  // allowed state is directly driven by stock ACC engagement. Permit the ACC message to come from either bus, in
  // order to accommodate future camera-side integrations if needed.
  if (addr == MSG_ACC_06) {
    int acc_status = (GET_BYTE(to_push,7) & 0x70) >> 4;
    controls_allowed = ((acc_status == 3) || (acc_status == 4) || (acc_status == 5)) ? 1 : 0;
  }
}

static int volkswagen_tx_hook(CAN_FIFOMailBox_TypeDef *to_send) {
  int addr = GET_ADDR(to_send);
  int tx = 1;

  // Safety check for HCA_01 Heading Control Assist torque.
  if (addr == MSG_HCA_01) {
    bool violation = false;

    int desired_torque = GET_BYTE(to_send, 2) | ((GET_BYTE(to_send, 3) & 0x3F) << 8);
    int sign = (GET_BYTE(to_send, 3) & 0x80) >> 7;
    if (sign == 1) {
      desired_torque *= -1;
    }

    uint32_t ts = TIM2->CNT;

    if (controls_allowed) {

      // *** global torque limit check ***
      violation |= max_limit_check(desired_torque, VW_MAX_STEER, -VW_MAX_STEER);

      // *** torque rate limit check ***
      violation |= driver_limit_check(desired_torque, vw_desired_torque_last, &vw_torque_driver,
        VW_MAX_STEER, VW_MAX_RATE_UP, VW_MAX_RATE_DOWN,
        VW_DRIVER_TORQUE_ALLOWANCE, VW_DRIVER_TORQUE_FACTOR);
      vw_desired_torque_last = desired_torque;

      // *** torque real time rate limit check ***
      violation |= rt_rate_limit_check(desired_torque, vw_rt_torque_last, VW_MAX_RT_DELTA);

      // every RT_INTERVAL set the new limits
      uint32_t ts_elapsed = get_ts_elapsed(ts, vw_ts_last);
      if (ts_elapsed > VW_RT_INTERVAL) {
        vw_rt_torque_last = desired_torque;
        vw_ts_last = ts;
      }
    }

    // no torque if controls is not allowed
    if (!controls_allowed && (desired_torque != 0)) {
      violation = true;
    }

    // reset to 0 if either controls is not allowed or there's a violation
    if (violation || !controls_allowed) {
      vw_desired_torque_last = 0;
      vw_rt_torque_last = 0;
      vw_ts_last = ts;
    }

    if (violation) {
      tx = 0;
    }
  }

  // 1 allows the message through
  return tx;
}

static int volkswagen_fwd_hook(int bus_num, CAN_FIFOMailBox_TypeDef *to_fwd) {
  int addr = GET_ADDR(to_fwd);
  int bus_fwd = -1;

  // NOTE: Will need refactoring for other bus layouts, such as no-forwarding at camera or J533 running-gear CAN

  switch(bus_num) {
    case 0:
      if(addr == MSG_GRA_ACC_01) {
        // OP intercepts, filters, and updates the cruise-control button messages before they reach the ACC radar.
        bus_fwd = -1;
      } else {
        // Forward all remaining traffic from J533 gateway to Extended CAN devices.
        bus_fwd = 2;
      }
      break;
    case 2:
      if((addr == MSG_HCA_01) || (addr == MSG_LDW_02)) {
        // OP takes control of the Heading Control Assist and Lane Departure Warning messages from the camera.
        bus_fwd = -1;
      } else {
        // Forward all remaining traffic from Extended CAN devices to J533 gateway.
        bus_fwd = 0;
      }
      break;
    default:
      // No other buses should be in use; fallback to do-not-forward.
      bus_fwd = -1;
      break;
  }

  return bus_fwd;
}

// While we do monitor VW Terminal 15 (ignition-on) state, we are not currently acting on it. Instead we use the
// default GPIO ignition hook. We may do so in the future for harness integrations at the camera (where we only have
// T30 unswitched power) instead of the gateway (where we have both T30 and T15 ignition-switched power).

const safety_hooks volkswagen_hooks = {
  .init = volkswagen_init,
  .rx = volkswagen_rx_hook,
  .tx = volkswagen_tx_hook,
  .tx_lin = nooutput_tx_lin_hook,
  .ignition = default_ign_hook,
  .fwd = volkswagen_fwd_hook,
};
