#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef STM32H7
#include "stm32h7xx.h"
#endif

// Common system headers
#include "board/sys/critical.h"
#include "board/sys/faults.h"
#include "board/sys/power_saving.h"
