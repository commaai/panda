#ifndef PANDA_EARLY_H
#define PANDA_EARLY_H

#include "rev.h"

#define ENTER_BOOTLOADER_MAGIC 0xdeadbeef
#define POST_BOOTLOADER_MAGIC 0xdeadb111
#define PULL_EFFECTIVE_DELAY 10

extern uint32_t enter_bootloader_mode;
extern void *_app_start[];
extern void *g_pfnVectors;
extern int has_external_debug_serial;
extern int is_giant_panda;
extern enum rev revision;

void spi_flasher();
void detect();
void early();

#endif
