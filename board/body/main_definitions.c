#include "board/config.h"
#include "board/main_declarations.h"

// body specific?
void debug_ring_callback(uart_ring *ring) {
  char rcv;
  while (get_char(ring, &rcv)) {
    (void)injectc(ring, rcv);
  }
}
