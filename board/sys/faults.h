#ifndef BOARD_SYS_FAULTS_H
#define BOARD_SYS_FAULTS_H

#include "board/sys/sys.h"

static uint8_t fault_status = FAULT_STATUS_NONE;
static uint32_t faults = 0U;

// sets the fault bits without logging them
static void fault_set(uint32_t fault) {
  if ((faults & fault) == 0U) {
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

static void fault_occurred(uint32_t fault) {
  if ((faults & fault) == 0U) {
    print("Fault occurred: 0x"); puth(fault); print("\n");
  }
  fault_set(fault);
}

static void print_line_number(uint32_t line) {
  const char digits[] = "0123456789";
  char str[11] = {0};  // the 10 digits of UINT32_MAX and the terminator
  uint8_t pos = 10U;
  uint32_t rest = line;
  do {
    pos--;
    str[pos] = digits[rest % 10U];
    rest /= 10U;
  } while (rest != 0U);
  print(&str[pos]);
}

#define FAULT_SITES 32U

// printed once per call site
static void fault_occurred_at(uint32_t fault, uint32_t value, const char *file, uint32_t line) {
  static struct {
    const char *file;
    uint32_t line;
  } sites[FAULT_SITES];
  static uint8_t site_cnt = 0U;
  bool seen = false;
  for (uint8_t i = 0U; i < site_cnt; i++) {
    seen = seen || ((sites[i].file == file) && (sites[i].line == line));
  }
  if (!seen && (site_cnt < FAULT_SITES)) {
    sites[site_cnt].file = file;
    sites[site_cnt].line = line;
    site_cnt++;
    print("Fault 0x"); puth(fault); print(" at "); print(file); print(":"); print_line_number(line); print(" value 0x"); puth(value); print("\n");
  }
  fault_set(fault);
}

// permanent faults stay set until reset
static inline void fault_recovered(uint32_t fault) {
  faults &= ~(fault & ~PERMANENT_FAULTS);
}

#endif  // BOARD_SYS_FAULTS_H
