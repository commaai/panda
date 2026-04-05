#include "board/config.h"
#include "board/bootstub_declarations.h"

// ******************** Prototypes ********************
void print(const char *a){ UNUSED(a); }
void puth(unsigned int i){ UNUSED(i); }
void puth2(unsigned int i){ UNUSED(i); }
void hexdump(const void *a, int l){ UNUSED(a); UNUSED(l); }

void uart_init(uart_ring *q, unsigned int baud) { UNUSED(q); UNUSED(baud); }
void debug_ring_callback(uart_ring *ring){ UNUSED(ring); }
