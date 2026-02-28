#ifdef BOOTSTUB
// DRIVER_BUILD is defined here because this file is compiled with bs_env (not bs_drv_env)
// which doesn't have -DDRIVER_BUILD in its CFLAGS. Other bootstub driver files use bs_drv_env.
#define DRIVER_BUILD
#include "board/config.h"

uint8_t hw_type = 0;
board *current_board;

#ifdef PANDA_JUNGLE
uint8_t harness_orientation = 0U;
uint8_t can_mode = 0U;
uint8_t ignition = 0U;
#endif

uart_ring uart_ring_som_debug;

void print(const char *a){ UNUSED(a); }
void puth(unsigned int i){ UNUSED(i); }
void puth2(unsigned int i){ UNUSED(i); }
void puth4(unsigned int i){ UNUSED(i); }
void puthx(uint32_t i, uint8_t len) { UNUSED(i); UNUSED(len); }
void hexdump(const void *a, int l){ UNUSED(a); UNUSED(l); }
void putch(const char a) { UNUSED(a); }
void uart_init(uart_ring *q, unsigned int baud) { UNUSED(q); UNUSED(baud); }
#endif
