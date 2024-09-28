#pragma once

#include "safety_declarations.h"
#include "can.h"

// include the safety policies.
#include "safety/safety_defaults.h"
#include "safety/safety_elm327.h"
#ifdef EXTRA_SAFETY_CONFIGS
#include "extra_safety_configs.h"
#endif

// from cereal.car.CarParams.SafetyModel
// defaults safety modes
#define SAFETY_SILENT 0U
#define SAFETY_ELM327 3U
#define SAFETY_ALLOUTPUT 17U
#define SAFETY_NOOUTPUT 19U

const int MAX_WRONG_COUNTERS = 5;

// This can be set by the safety hooks
bool controls_allowed = false;
bool relay_malfunction = false;
bool gas_pressed = false;
bool gas_pressed_prev = false;
bool brake_pressed = false;
bool brake_pressed_prev = false;
bool regen_braking = false;
bool regen_braking_prev = false;
bool cruise_engaged_prev = false;
struct sample_t vehicle_speed;
bool vehicle_moving = false;
bool acc_main_on = false;  // referred to as "ACC off" in ISO 15622:2018
int cruise_button_prev = 0;
bool safety_rx_checks_invalid = false;

// for safety modes with torque steering control
int desired_torque_last = 0;       // last desired steer torque
int rt_torque_last = 0;            // last desired torque for real time check
int valid_steer_req_count = 0;     // counter for steer request bit matching non-zero torque
int invalid_steer_req_count = 0;   // counter to allow multiple frames of mismatching torque request bit
struct sample_t torque_meas;       // last 6 motor torques produced by the eps
struct sample_t torque_driver;     // last 6 driver torques measured
uint32_t ts_torque_check_last = 0;
uint32_t ts_steer_req_mismatch_last = 0;  // last timestamp steer req was mismatched with torque

// state for controls_allowed timeout logic
bool heartbeat_engaged = false;             // openpilot enabled, passed in heartbeat USB command
uint32_t heartbeat_engaged_mismatches = 0;  // count of mismatches between heartbeat_engaged and controls_allowed

// for safety modes with angle steering control
uint32_t ts_angle_last = 0;
int desired_angle_last = 0;
struct sample_t angle_meas;         // last 6 steer angles/curvatures


int alternative_experience = 0;

// time since safety mode has been changed
uint32_t safety_mode_cnt = 0U;

uint16_t current_safety_mode = SAFETY_SILENT;
uint16_t current_safety_param = 0;
static const safety_hooks *current_hooks = &nooutput_hooks;
safety_config current_safety_config;

static bool is_msg_valid(RxCheck addr_list[], int index) {
  bool valid = true;
  if (index != -1) {
    if (!addr_list[index].status.valid_checksum || !addr_list[index].status.valid_quality_flag || (addr_list[index].status.wrong_counters >= MAX_WRONG_COUNTERS)) {
      valid = false;
      controls_allowed = false;
    }
  }
  return valid;
}

static int get_addr_check_index(const CANPacket_t *to_push, RxCheck addr_list[], const int len) {
  int bus = GET_BUS(to_push);
  int addr = GET_ADDR(to_push);
  int length = GET_LEN(to_push);

  int index = -1;
  for (int i = 0; i < len; i++) {
    // if multiple msgs are allowed, determine which one is present on the bus
    if (!addr_list[i].status.msg_seen) {
      for (uint8_t j = 0U; (j < MAX_ADDR_CHECK_MSGS) && (addr_list[i].msg[j].addr != 0); j++) {
        if ((addr == addr_list[i].msg[j].addr) && (bus == addr_list[i].msg[j].bus) &&
              (length == addr_list[i].msg[j].len)) {
          addr_list[i].status.index = j;
          addr_list[i].status.msg_seen = true;
          break;
        }
      }
    }

    if (addr_list[i].status.msg_seen) {
      int idx = addr_list[i].status.index;
      if ((addr == addr_list[i].msg[idx].addr) && (bus == addr_list[i].msg[idx].bus) &&
          (length == addr_list[i].msg[idx].len)) {
        index = i;
        break;
      }
    }
  }
  return index;
}

static void update_addr_timestamp(RxCheck addr_list[], int index) {
  if (index != -1) {
    uint32_t ts = microsecond_timer_get();
    addr_list[index].status.last_timestamp = ts;
  }
}

static void update_counter(RxCheck addr_list[], int index, uint8_t counter) {
  if (index != -1) {
    uint8_t expected_counter = (addr_list[index].status.last_counter + 1U) % (addr_list[index].msg[addr_list[index].status.index].max_counter + 1U);
    addr_list[index].status.wrong_counters += (expected_counter == counter) ? -1 : 1;
    addr_list[index].status.wrong_counters = CLAMP(addr_list[index].status.wrong_counters, 0, MAX_WRONG_COUNTERS);
    addr_list[index].status.last_counter = counter;
  }
}

static bool rx_msg_safety_check(const CANPacket_t *to_push,
                         const safety_config *cfg,
                         const safety_hooks *safety_hooks) {

  int index = get_addr_check_index(to_push, cfg->rx_checks, cfg->rx_checks_len);
  update_addr_timestamp(cfg->rx_checks, index);

  if (index != -1) {
    // checksum check
    if ((safety_hooks->get_checksum != NULL) && (safety_hooks->compute_checksum != NULL) && cfg->rx_checks[index].msg[cfg->rx_checks[index].status.index].check_checksum) {
      uint32_t checksum = safety_hooks->get_checksum(to_push);
      uint32_t checksum_comp = safety_hooks->compute_checksum(to_push);
      cfg->rx_checks[index].status.valid_checksum = checksum_comp == checksum;
    } else {
      cfg->rx_checks[index].status.valid_checksum = true;
    }

    // counter check (max_counter == 0 means skip check)
    if ((safety_hooks->get_counter != NULL) && (cfg->rx_checks[index].msg[cfg->rx_checks[index].status.index].max_counter > 0U)) {
      uint8_t counter = safety_hooks->get_counter(to_push);
      update_counter(cfg->rx_checks, index, counter);
    } else {
      cfg->rx_checks[index].status.wrong_counters = 0U;
    }

    // quality flag check
    if ((safety_hooks->get_quality_flag_valid != NULL) && cfg->rx_checks[index].msg[cfg->rx_checks[index].status.index].quality_flag) {
      cfg->rx_checks[index].status.valid_quality_flag = safety_hooks->get_quality_flag_valid(to_push);
    } else {
      cfg->rx_checks[index].status.valid_quality_flag = true;
    }
  }
  return is_msg_valid(cfg->rx_checks, index);
}

bool safety_rx_hook(const CANPacket_t *to_push) {
  bool controls_allowed_prev = controls_allowed;

  bool valid = rx_msg_safety_check(to_push, &current_safety_config, current_hooks);
  if (valid) {
    current_hooks->rx(to_push);
  }

  // reset mismatches on rising edge of controls_allowed to avoid rare race condition
  if (controls_allowed && !controls_allowed_prev) {
    heartbeat_engaged_mismatches = 0;
  }

  return valid;
}

static bool msg_allowed(const CANPacket_t *to_send, const CanMsg msg_list[], int len) {
  int addr = GET_ADDR(to_send);
  int bus = GET_BUS(to_send);
  int length = GET_LEN(to_send);

  bool allowed = false;
  for (int i = 0; i < len; i++) {
    if ((addr == msg_list[i].addr) && (bus == msg_list[i].bus) && (length == msg_list[i].len)) {
      allowed = true;
      break;
    }
  }
  return allowed;
}

bool safety_tx_hook(CANPacket_t *to_send) {
  bool whitelisted = msg_allowed(to_send, current_safety_config.tx_msgs, current_safety_config.tx_msgs_len);
  if ((current_safety_mode == SAFETY_ALLOUTPUT) || (current_safety_mode == SAFETY_ELM327)) {
    whitelisted = true;
  }

  const bool safety_allowed = current_hooks->tx(to_send);
  return !relay_malfunction && whitelisted && safety_allowed;
}

int safety_fwd_hook(int bus_num, int addr) {
  return (relay_malfunction ? -1 : current_hooks->fwd(bus_num, addr));
}

bool get_longitudinal_allowed(void) {
  return controls_allowed && !gas_pressed_prev;
}

// 1Hz safety function called by main. Now just a check for lagging safety messages
void safety_tick(const safety_config *cfg) {
  const uint8_t MAX_MISSED_MSGS = 10U;
  bool rx_checks_invalid = false;
  uint32_t ts = microsecond_timer_get();
  if (cfg != NULL) {
    for (int i=0; i < cfg->rx_checks_len; i++) {
      uint32_t elapsed_time = get_ts_elapsed(ts, cfg->rx_checks[i].status.last_timestamp);
      // lag threshold is max of: 1s and MAX_MISSED_MSGS * expected timestep.
      // Quite conservative to not risk false triggers.
      // 2s of lag is worse case, since the function is called at 1Hz
      uint32_t timestep = 1e6 / cfg->rx_checks[i].msg[cfg->rx_checks[i].status.index].frequency;
      bool lagging = elapsed_time > MAX(timestep * MAX_MISSED_MSGS, 1e6);
      cfg->rx_checks[i].status.lagging = lagging;
      if (lagging) {
        controls_allowed = false;
      }

      if (lagging || !is_msg_valid(cfg->rx_checks, i)) {
        rx_checks_invalid = true;
      }
    }
  }

  safety_rx_checks_invalid = rx_checks_invalid;
}

static void relay_malfunction_reset(void) {
  relay_malfunction = false;
  fault_recovered(FAULT_RELAY_MALFUNCTION);
}

// resets values and min/max for sample_t struct
static void reset_sample(struct sample_t *sample) {
  for (int i = 0; i < MAX_SAMPLE_VALS; i++) {
    sample->values[i] = 0;
  }
  update_sample(sample, 0);
}

int set_safety_hooks(uint16_t mode, uint16_t param) {
  const safety_hook_config safety_hook_registry[] = {
    {SAFETY_SILENT, &nooutput_hooks},
    {SAFETY_NOOUTPUT, &nooutput_hooks},
    {SAFETY_ELM327, &elm327_hooks},
#ifdef ALLOW_DEBUG
    {SAFETY_ALLOUTPUT, &alloutput_hooks},
#endif
#ifdef EXTRA_SAFETY_CONFIGS
    GET_EXTRA_SAFETY_HOOKS
#endif
  };

  // reset state set by safety mode
  safety_mode_cnt = 0U;
  relay_malfunction = false;
  gas_pressed = false;
  gas_pressed_prev = false;
  brake_pressed = false;
  brake_pressed_prev = false;
  regen_braking = false;
  regen_braking_prev = false;
  cruise_engaged_prev = false;
  vehicle_moving = false;
  acc_main_on = false;
  cruise_button_prev = 0;
  desired_torque_last = 0;
  rt_torque_last = 0;
  ts_angle_last = 0;
  desired_angle_last = 0;
  ts_torque_check_last = 0;
  ts_steer_req_mismatch_last = 0;
  valid_steer_req_count = 0;
  invalid_steer_req_count = 0;

  // reset samples
  reset_sample(&vehicle_speed);
  reset_sample(&torque_meas);
  reset_sample(&torque_driver);
  reset_sample(&angle_meas);

  controls_allowed = false;
  relay_malfunction_reset();
  safety_rx_checks_invalid = false;

  current_safety_config.rx_checks = NULL;
  current_safety_config.rx_checks_len = 0;
  current_safety_config.tx_msgs = NULL;
  current_safety_config.tx_msgs_len = 0;

  int set_status = -1;  // not set
  int hook_config_count = sizeof(safety_hook_registry) / sizeof(safety_hook_config);
  for (int i = 0; i < hook_config_count; i++) {
    if (safety_hook_registry[i].id == mode) {
      current_hooks = safety_hook_registry[i].hooks;
      current_safety_mode = mode;
      current_safety_param = param;
      set_status = 0;  // set
    }
  }
  if ((set_status == 0) && (current_hooks->init != NULL)) {
    safety_config cfg = current_hooks->init(param);
    current_safety_config.rx_checks = cfg.rx_checks;
    current_safety_config.rx_checks_len = cfg.rx_checks_len;
    current_safety_config.tx_msgs = cfg.tx_msgs;
    current_safety_config.tx_msgs_len = cfg.tx_msgs_len;
    // reset all dynamic fields in addr struct
    for (int j = 0; j < current_safety_config.rx_checks_len; j++) {
      current_safety_config.rx_checks[j].status = (RxStatus){0};
    }
  }
  return set_status;
}

// given a new sample, update the sample_t struct
void update_sample(struct sample_t *sample, int sample_new) {
  for (int i = MAX_SAMPLE_VALS - 1; i > 0; i--) {
    sample->values[i] = sample->values[i-1];
  }
  sample->values[0] = sample_new;

  // get the minimum and maximum measured samples
  sample->min = sample->values[0];
  sample->max = sample->values[0];
  for (int i = 1; i < MAX_SAMPLE_VALS; i++) {
    if (sample->values[i] < sample->min) {
      sample->min = sample->values[i];
    }
    if (sample->values[i] > sample->max) {
      sample->max = sample->values[i];
    }
  }
}

int ROUND(float val) {
  return val + ((val > 0.0) ? 0.5 : -0.5);
}
