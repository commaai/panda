#include "utils.h"
#include <stdint.h>

uint32_t get_ts_elapsed(uint32_t ts, uint32_t ts_last) {
  return ts - ts_last;
}