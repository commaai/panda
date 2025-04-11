#pragma once

static inline void unused_init_bootloader(void) {
}

static inline void unused_set_ir_power(uint8_t percentage) {
  UNUSED(percentage);
}

static inline void unused_set_fan_enabled(bool enabled) {
  UNUSED(enabled);
}

static inline void unused_set_siren(bool enabled) {
  UNUSED(enabled);
}

static inline uint32_t unused_read_current(void) {
  return 0U;
}

static inline void unused_set_bootkick(BootState state) {
  UNUSED(state);
}

static inline bool unused_read_som_gpio(void) {
  return false;
}

static inline void unused_set_amp_enabled(bool enabled) {
  UNUSED(enabled);
}
