#include <stdint.h>

#include "simple_watchdog.h"

#include "board/drivers/timers.h"
#include "board/utils.h"
#include "board/libc.h"
#include "board/faults.h"

simple_watchdog_state_t wd_state;

