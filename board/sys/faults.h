#pragma once

#include "board/sys/sys.h"

uint8_t fault_status = FAULT_STATUS_NONE;
uint32_t faults = 0U;

void fault_occurred(uint32_t fault) {
  if ((faults & fault) == 0U) {
    if ((PERMANENT_FAULTS & fault) != 0U) {
      print("Permanent fault occurred: 0x"); puth(fault); print("\n");
      fault_status = FAULT_STATUS_PERMANENT;
      fault_safe_state();
    } else {
      print("Temporary fault occurred: 0x"); puth(fault); print("\n");
      fault_status = (fault_status == FAULT_STATUS_PERMANENT) ? FAULT_STATUS_PERMANENT : FAULT_STATUS_TEMPORARY;
    }
  }
  faults |= fault;
}

#define FAULT_SITES 32U

// each call site logs once with its address, noinline so it's the caller's
__attribute__((noinline)) void fault_occurred_value(uint32_t fault, uint32_t value) {
  static uint32_t sites[FAULT_SITES];
  static uint8_t site_cnt = 0U;
  // cppcheck-suppress misra-c2012-17.3 ; compiler builtin
  uint32_t site = (uint32_t)__builtin_return_address(0);
  bool seen = false;
  for (uint8_t i = 0U; i < site_cnt; i++) {
    seen = seen || (sites[i] == site);
  }
  if (!seen && (site_cnt < FAULT_SITES)) {
    sites[site_cnt] = site;
    site_cnt++;
    print("Fault 0x"); puth(fault); print(" at 0x"); puth(site); print(" value 0x"); puth(value); print("\n");
  }
  fault_occurred(fault);
}

void fault_recovered(uint32_t fault) {
  if ((PERMANENT_FAULTS & fault) == 0U) {
    faults &= ~fault;
  } else {
    print("Cannot recover from a permanent fault!\n");
  }
}
