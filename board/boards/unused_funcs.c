#include "board/config.h"
#include "unused_funcs.h"

void unused_set_fan_enabled(bool enabled) { (void)enabled; }
void unused_set_siren(bool enabled) { (void)enabled; }
void unused_set_bootkick(BootState state) { (void)state; }
void unused_set_ir_power(uint8_t percentage) { (void)percentage; }
bool unused_read_som_gpio(void) { return false; }
void unused_set_amp_enabled(bool enabled) { (void)enabled; }
uint32_t unused_read_voltage_mV(void) { return 0U; }
uint32_t unused_read_current_mA(void) { return 0U; }
int unused_comms_control_handler(ControlPacket_t *req, uint8_t *resp) { 
  (void)req; 
  (void)resp; 
  return 0; 
}
