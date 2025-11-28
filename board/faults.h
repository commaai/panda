#include <stdint.h>

#include "faults_declarations.h"

extern uint8_t fault_status;
extern uint32_t faults;

void fault_occurred(uint32_t fault);
void fault_recovered(uint32_t fault);