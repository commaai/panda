#define FAULT_STATUS_NONE 0U
#define FAULT_STATUS_TEMPORARY 1U
#define FAULT_STATUS_PERMANENT 2U

// Definition of all fault types
#define FAULT_RELAY_MALFUNCTION         (1 << 0)
#define FAULT_RELAY_MALFUNCTION         (1 << 1)

// Permanent faults
#define PERMANENT_FAULTS = 0U;

uint8_t fault_status = FAULT_STATUS_NONE;
uint32_t faults = 0U;

void fault_occurred(uint32_t fault) {
  faults |= fault;
  if((PERMANENT_FAULTS & fault) != 0U){
    puts("Permanent fault occurred: 0x"); puth(fault); puts("\n")
    fault_status = FAULT_STATUS_PERMANENT;
  } else {
    puts("Temporary fault occurred: 0x"); puth(fault); puts("\n")
    fault_status = FAULT_STATUS_TEMPORARY;
  }
}

void fault_recovered(uint32_t fault) {
  if((PERMANENT_FAULTS & fault) == 0U){
    faults &= ~fault;
  } else {
    puts("Cannot recover from a permanent fault!\n")
  }
}