typedef enum {
  WATCHDOG_50_MS = (400U - 1U),
  WATCHDOG_500_MS = 4000U,
} WatchdogTimeout;

static void watchdog_feed(void) {
  IND_WDG->KR = 0xAAAAU;
}
