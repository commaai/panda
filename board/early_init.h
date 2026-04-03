#pragma once

// Early bringup
#define ENTER_BOOTLOADER_MAGIC 0xdeadbeefU
#define ENTER_SOFTLOADER_MAGIC 0xdeadc0deU
#define BOOT_NORMAL 0xdeadb111U

extern uint32_t enter_bootloader_mode;

void early_initialization(void);
