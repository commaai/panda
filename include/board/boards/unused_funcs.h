#pragma once
#include "safety/board/utils.h"

static void unused_init_bootloader(void) {
}

static void unused_set_ir_power(uint8_t percentage) {
  UNUSED(percentage);
}

static void unused_set_fan_enabled(bool enabled) {
  UNUSED(enabled);
}

static void unused_set_siren(bool enabled) {
  UNUSED(enabled);
}

static uint32_t unused_read_current(void) {
  return 0U;
}

static void unused_set_bootkick(BootState state) {
  UNUSED(state);
}

static bool unused_read_som_gpio(void) {
  return false;
}

static void unused_set_amp_enabled(bool enabled) {
  UNUSED(enabled);
}
