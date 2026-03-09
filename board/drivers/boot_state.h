#ifndef DRIVERS_BOOT_STATE_H
#define DRIVERS_BOOT_STATE_H

// Boot state enum used by bootkick driver
// This is a minimal header to avoid circular dependencies
typedef enum {
  BOOT_STANDBY,
  BOOT_BOOTKICK,
  BOOT_RESET,
} BootState;

#endif
