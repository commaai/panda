#include "board/sys/sys.h"

uint8_t fault_status = FAULT_STATUS_NONE;
uint32_t faults = 0U;

void fault_occurred(uint32_t fault) {
  if ((faults & fault) == 0U) {
    print("Temporary fault occurred: 0x"); puth(fault); print("\n");
    fault_status = FAULT_STATUS_TEMPORARY;
  }
  faults |= fault;
}

void fault_recovered(uint32_t fault) {
  faults &= ~fault;
}
