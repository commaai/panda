#pragma once

// Boot state enum used by bootkick driver
typedef enum {
  BOOT_STANDBY,
  BOOT_BOOTKICK,
  BOOT_RESET,
} BootState;
