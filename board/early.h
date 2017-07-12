#define ENTER_BOOTLOADER_MAGIC 0xdeadbeef
#define POST_BOOTLOADER_MAGIC 0xdeadb111
#define PULL_EFFECTIVE_DELAY 10

#define PANDA_REV_AB 0
#define PANDA_REV_C 1

extern uint32_t enter_bootloader_mode;
extern void *_app_start[];
extern void *g_pfnVectors;
extern int has_external_debug_serial;
extern int is_giant_panda;
extern int revision;

void spi_flasher();
void detect();
void early();
