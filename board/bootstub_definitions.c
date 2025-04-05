#include "bootstub_definitions.h"

void print(const char *a){ UNUSED(a); }
void puth(uint8_t i){ UNUSED(i); }
void puth2(uint8_t i){ UNUSED(i); }
void puth4(uint8_t i){ UNUSED(i); }
void hexdump(const void *a, int l){ UNUSED(a); UNUSED(l); }
// No UART support in bootloader
//typedef struct uart_ring {} uart_ring;
uart_ring uart_ring_som_debug;
uart_ring uart_ring_debug;
uart_ring *get_ring_by_number(int a);
void uart_init(uart_ring *q, int baud) { UNUSED(q); UNUSED(baud); }
bool put_char(uart_ring *q, char elem);

// ********************* Globals **********************
uint8_t hw_type = 0;
board *current_board;

uint32_t uptime_cnt;
bool ignition_can;
bool heartbeat_lost;
bool bootkick_reset_triggered;
