#pragma once

#include <stdint.h>
#include <stdbool.h>

// ///////////////////////////////////// //
// Red Panda chiplet (STM32H7) + Harness //
// ///////////////////////////////////// //

// Most hardware functionality is similar to red panda

void red_chiplet_enable_can_transceiver(uint8_t transceiver, bool enabled);
void red_chiplet_enable_can_transceivers(bool enabled);
void red_chiplet_set_can_mode(uint8_t mode);
void red_chiplet_set_fan_or_usb_load_switch(bool enabled);
void red_chiplet_init(void);

extern harness_configuration red_chiplet_harness_config;
