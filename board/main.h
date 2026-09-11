#pragma once

#include <stdbool.h>
#include <stdint.h>

// ********************* Globals **********************
extern uint32_t uptime_cnt;

// heartbeat state
extern uint32_t heartbeat_counter;
extern bool heartbeat_lost;
extern bool heartbeat_disabled;

// siren state
extern bool siren_enabled;

#define ENTER_BOOTLOADER_MAGIC 0xdeadbeefU
#define ENTER_SOFTLOADER_MAGIC 0xdeadc0deU
#define BOOT_NORMAL 0xdeadb111U
#define PROVISION_CHUNK_LEN 0x20

extern uint32_t enter_bootloader_mode;
void early_initialization(void);
void get_provision_chunk(uint8_t *resp);
void soft_flasher_start(void);
void set_safety_mode(uint16_t mode, uint16_t param);
bool is_car_safety_mode(uint16_t mode);
#ifdef PANDA_JUNGLE
extern bool generated_can_traffic;
#endif
