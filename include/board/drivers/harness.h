#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "drivers/harness_configuration.h"
#include "board/boards/board.h"

extern board *current_board;
extern uint16_t adc_get_mV(uint8_t channel);

#define HARNESS_STATUS_NC 0U
#define HARNESS_STATUS_NORMAL 1U
#define HARNESS_STATUS_FLIPPED 2U

struct harness_t {
  uint8_t status;
  uint16_t sbu1_voltage_mV;
  uint16_t sbu2_voltage_mV;
  bool relay_driven;
  bool sbu_adc_lock;
};
extern struct harness_t harness;


// The ignition relay is only used for testing purposes
void set_intercept_relay(bool intercept, bool ignition_relay);
bool harness_check_ignition(void);
void harness_tick(void);
void harness_init(void);
