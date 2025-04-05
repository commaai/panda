#pragma once

#include <stdint.h>

extern void early_gpio_float(void);
extern void detect_board_type(void);

#define ENTER_BOOTLOADER_MAGIC 0xdeadbeefU
#define ENTER_SOFTLOADER_MAGIC 0xdeadc0deU
#define BOOT_NORMAL 0xdeadb111U

extern void *g_pfnVectors;
extern uint32_t enter_bootloader_mode;

typedef void (*bootloader_fcn)(void);
typedef bootloader_fcn *bootloader_fcn_ptr;

void jump_to_bootloader(void); // TODO: Should this be static?
void early_initialization(void);
