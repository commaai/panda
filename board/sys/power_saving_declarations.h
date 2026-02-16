#pragma once

// WARNING: To stay in compliance with the SIL2 rules laid out in STM UM1840, we should never implement any of the available hardware low power modes.
// See rule: CoU_3

extern bool power_save_enabled;

void set_power_save_state(bool enable);
