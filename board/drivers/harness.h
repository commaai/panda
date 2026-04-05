#ifndef PANDA_DRIVERS_HARNESS_H
#define PANDA_DRIVERS_HARNESS_H

#include <stdbool.h>
#include <stdint.h>
#include "board/drivers/driver_declarations.h"

#ifdef STM32H7
#include "board/stm32h7/lladc.h"
#endif

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

void set_intercept_relay(bool intercept, bool ignition_relay);
bool harness_check_ignition(void);
void harness_tick(void);
void harness_init(void);

#endif
