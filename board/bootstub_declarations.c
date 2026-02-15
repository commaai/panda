#include "board/boards/board_declarations.h"
#include "board/bootstub_declarations.h"

void print(const char *a){ UNUSED(a); }
void puth(uint8_t i){ UNUSED(i); }
void puth2(uint8_t i){ UNUSED(i); }
void puth4(uint8_t i){ UNUSED(i); }
void hexdump(const void *a, int l){ UNUSED(a); UNUSED(l); }

uart_ring uart_ring_som_debug;
void uart_init(uart_ring *q, int baud) { UNUSED(q); UNUSED(baud); }

uint8_t hw_type = 0;
board *current_board;
