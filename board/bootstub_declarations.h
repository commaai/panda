#ifndef BOARD_BOOTSTUB_DECLARATIONS_H
#define BOARD_BOOTSTUB_DECLARATIONS_H

// ******************** Prototypes ********************
static void print(const char *a){ UNUSED(a); }
static void puth(unsigned int i){ UNUSED(i); }
static void hexdump(const uint8_t *a, int l){ UNUSED(a); UNUSED(l); }
__attribute__((unused)) static void puth4(unsigned int i){ UNUSED(i); }
typedef struct board board;
typedef struct harness_configuration harness_configuration;

// ********************* Globals **********************
uint8_t hw_type = 0;
const board *current_board;

#endif  // BOARD_BOOTSTUB_DECLARATIONS_H
