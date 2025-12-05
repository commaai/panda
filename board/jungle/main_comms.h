#include "board/config.h"

#include "board/globals.h"

extern bool generated_can_traffic;
extern bool panda_power;

// send on serial, first byte to select the ring
void comms_endpoint2_write(const uint8_t *data, uint32_t len);
int comms_control_handler(ControlPacket_t *req, uint8_t *resp);