#ifndef BOARD_SYS_FAULTS_H
#define BOARD_SYS_FAULTS_H

#include "board/sys/sys.h"

static uint8_t fault_status = FAULT_STATUS_NONE;
static uint32_t faults = 0U;

static void fault_occurred(uint32_t fault) {
  if ((faults & fault) == 0U) {
    print("Fault occurred: 0x"); puth(fault); print("\n");
    if (fault_status == FAULT_STATUS_NONE) {
      fault_status = FAULT_STATUS_TEMPORARY;
    }
#if PERMANENT_FAULTS != 0U
    if ((PERMANENT_FAULTS & fault) != 0U) {
      fault_status = FAULT_STATUS_PERMANENT;
      fault_safe_state();
    }
#endif
  }
  faults |= fault;
}

#define FAULT_SITES 32U

// printed once per call site
void fault_occurred_at(uint32_t fault, uint32_t value, const char *site) {
  static const char *sites[FAULT_SITES];
  static uint8_t site_cnt = 0U;
  bool seen = false;
  for (uint8_t i = 0U; i < site_cnt; i++) {
    seen = seen || (sites[i] == site);
  }
  if (!seen && (site_cnt < FAULT_SITES)) {
    sites[site_cnt] = site;
    site_cnt++;
    print("Fault 0x"); puth(fault); print(" at "); print(site); print(" value 0x"); puth(value); print("\n");
  }
  fault_occurred(fault);
}

// permanent faults stay set until reset
static inline void fault_recovered(uint32_t fault) {
  faults &= ~(fault & ~PERMANENT_FAULTS);
}

#endif  // BOARD_SYS_FAULTS_H
