// Main communications interface
#pragma once

extern int _app_start[0xc000];

// Core function prototypes
void set_safety_mode(uint16_t mode, uint16_t param);
bool is_car_safety_mode(uint16_t mode);

// Health packet functions
static int get_health_pkt(void *dat);

// Endpoint functions
void comms_endpoint2_write(const uint8_t *data, uint32_t len);
int comms_control_handler(ControlPacket_t *req, uint8_t *resp);

// Additional prototypes
uint8_t get_board_id(void);
uint8_t get_usb_power_mode(void);

// USB control request handling
unsigned int get_usb_connection_status(void);
void handle_serial_msg(unsigned char *data, unsigned int len);
void can_tx_comms_resume_spi(void);
