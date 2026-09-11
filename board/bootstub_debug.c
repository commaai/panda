#include "board/config.h"
#include "board/drivers/drivers.h"

void print(const char *a){ UNUSED(a); }
void puth(unsigned int i){ UNUSED(i); }
void puth4(unsigned int i){ UNUSED(i); }
void hexdump(const void *a, int l){ UNUSED(a); UNUSED(l); }
// No UART support in bootloader
void uart_init(uart_ring *q, unsigned int baud) { UNUSED(q); UNUSED(baud); }

uart_ring uart_ring_som_debug;
