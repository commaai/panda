#pragma once
#include "drivers/harness_configuration.h"
#include "boards/board.h"
// ******************** Prototypes ********************
extern void print(const char *a);
extern void puth(uint8_t i);
extern void puth2(uint8_t i);
extern void puth4(uint8_t i);
extern void hexdump(const void *a, int l);

// No UART support in bootloader
typedef struct uart_ring {} uart_ring;
extern uart_ring uart_ring_som_debug;
extern uart_ring uart_ring_debug;
extern uart_ring *get_ring_by_number(int a);
extern void uart_init(uart_ring *q, int baud);
extern bool put_char(uart_ring *q, char elem);

// Varian CAN-related constants not needed in bootloader
extern uint32_t uptime_cnt; // TODO: should this get a value?
extern bool ignition_can;
extern bool heartbeat_lost;
extern bool bootkick_reset_triggered;
extern const unsigned char dlc_to_len[];

// ********************* Globals **********************
extern uint8_t hw_type;
extern board *current_board;

