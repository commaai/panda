#ifndef EARLY_INIT_H
#define EARLY_INIT_H

#include "board/config.h"

#define ENTER_BOOTLOADER_MAGIC 0xdeadbeefU
#define ENTER_SOFTLOADER_MAGIC 0xdeadc0deU
#define BOOT_NORMAL 0xdeadb111U

extern void *g_pfnVectors;

typedef void (*bootloader_fcn)(void);
// cppcheck-suppress misra-c2012-2.3 ; used in driver implementations
typedef bootloader_fcn *bootloader_fcn_ptr;

void early_initialization(void);

#endif
