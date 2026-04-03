#pragma once

#include "board/drivers/drivers.h"

extern struct harness_t harness;

// The ignition relay is only used for testing purposes
void set_intercept_relay(bool intercept, bool ignition_relay);
bool harness_check_ignition(void);
void harness_tick(void);
void harness_init(void);
