#pragma once

typedef enum {
  WATCHDOG_50_MS = (400U - 1U),
  WATCHDOG_500_MS = 4000U,
} WatchdogTimeout;

void watchdog_init(WatchdogTimeout timeout);
