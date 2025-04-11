#pragma once
#include <stdint.h>

#define ENTER_BOOTLOADER_MAGIC 0xdeadbeefU
#define ENTER_SOFTLOADER_MAGIC 0xdeadc0deU
#define BOOT_NORMAL 0xdeadb111U

extern void *g_pfnVectors;
extern uint32_t enter_bootloader_mode;

typedef void (*bootloader_fcn)(void);
typedef bootloader_fcn *bootloader_fcn_ptr;

void early_initialization(void);
