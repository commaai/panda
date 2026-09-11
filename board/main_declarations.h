#pragma once

#include "board/drivers/drivers.h"

// ******************** Prototypes ********************
typedef struct board board;

void set_safety_mode(uint16_t mode, uint16_t param);
bool is_car_safety_mode(uint16_t mode);

// ********************* Globals **********************
extern uint8_t hw_type;
extern board *current_board;
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

extern void *g_pfnVectors;
extern uint32_t enter_bootloader_mode;

void early_initialization(void);
void detect_board_type(void);

#define PROVISION_CHUNK_LEN 0x20
void get_provision_chunk(uint8_t *resp);

#ifdef BOOTSTUB
extern void *_app_start[];
#else
extern int _app_start[0xc000]; // First three application sectors
#endif
#ifdef PANDA_JUNGLE
extern bool generated_can_traffic;
#endif
