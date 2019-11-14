#define FAULT_STATUS_NONE 0U
#define FAULT_STATUS_FAULT 1U

uint8_t fault_status = FAULT_STATUS_NONE;

void fault_occurred(void) {
  puts("Fault occurred!\n");
  fault_status = FAULT_STATUS_FAULT;
}