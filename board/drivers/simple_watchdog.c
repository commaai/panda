#include "board/drivers/simple_watchdog.h"

#include <stdbool.h>

#include "stm32h7xx.h"

#include "board/main_declarations.h"
#include "board/sys/sys.h"

typedef struct simple_watchdog_state_t {
  uint32_t fault;
  uint32_t last_ts;
  uint32_t threshold;
} simple_watchdog_state_t;

uint32_t get_ts_elapsed(uint32_t ts, uint32_t ts_last);
uint32_t microsecond_timer_get(void);

static simple_watchdog_state_t wd_state;

void simple_watchdog_kick(void) {
  uint32_t ts = microsecond_timer_get();

  uint32_t et = get_ts_elapsed(ts, wd_state.last_ts);
  if (et > wd_state.threshold) {
    print("WD timeout 0x"); puth(et); print("\n");
    fault_occurred(wd_state.fault);
  }

  wd_state.last_ts = ts;
}

void simple_watchdog_init(uint32_t fault, uint32_t threshold) {
  wd_state.fault = fault;
  wd_state.threshold = threshold;
  wd_state.last_ts = microsecond_timer_get();
}
