#include "board/config.h"

extern int _app_start[0xc000]; // Only first 3 sectors of size 0x4000 are used


#include "board/globals.h"

// TODO:
extern bool generated_can_traffic;
extern bool panda_power;


// send on serial, first byte to select the ring
void comms_endpoint2_write(const uint8_t *data, uint32_t len);
int comms_control_handler(ControlPacket_t *req, uint8_t *resp);