// ******************** Prototypes ********************
void puts(const char *a){ UNUSED(a); }
void puth(uint8_t i){ UNUSED(i); }
void puth2(uint8_t i){ UNUSED(i); }
typedef struct board board;
typedef struct harness_configuration harness_configuration;
// No CAN support on bootloader
void can_flip_buses(uint8_t bus1, uint8_t bus2){UNUSED(bus1); UNUSED(bus2);}
void can_set_obd(uint8_t harness_orientation, bool obd){UNUSED(harness_orientation); UNUSED(obd);}

// ********************* Globals **********************
uint8_t hw_type = 0;
const board *current_board;
