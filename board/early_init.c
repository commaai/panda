#include <stdbool.h>
#include <stdint.h>
#include "stm32h7xx.h"
#include "stm32h7xx_hal_gpio_ex.h"
#include "board/drivers/drivers.h"

// Forward declarations for functions defined in headers included by main.c
#define LED_RED 0U
#define LED_GREEN 1U
#define LED_BLUE 2U
void led_init(void);
void led_set(uint8_t color, bool enabled);
void early_gpio_float(void);
void detect_board_type(void);

// Board struct definitions (needed for current_board->init_bootloader())
#ifdef PANDA_JUNGLE
  #include "board/jungle/boards/board_declarations.h"
#elif defined(PANDA_BODY)
  #include "board/body/boards/board_declarations.h"
#else
  #include "board/boards/board_declarations.h"
#endif
extern struct board *current_board;

// Forward declarations from sys/critical.h
void enable_interrupts(void);
void disable_interrupts(void);
extern uint8_t global_critical_depth;

// Constants from stm32h7_config.h
#define BOOTLOADER_ADDRESS 0x1FF09804U
#define MCU_IDCODE 0x483U

// Early bringup
#define ENTER_BOOTLOADER_MAGIC 0xdeadbeefU
#define ENTER_SOFTLOADER_MAGIC 0xdeadc0deU
#define BOOT_NORMAL 0xdeadb111U

extern void *g_pfnVectors;
extern uint32_t enter_bootloader_mode;

typedef void (*bootloader_fcn)(void);
typedef bootloader_fcn *bootloader_fcn_ptr;

static void jump_to_bootloader(void) {
  // do enter bootloader
  enter_bootloader_mode = 0;

  bootloader_fcn_ptr bootloader_ptr = (bootloader_fcn_ptr)BOOTLOADER_ADDRESS;
  bootloader_fcn bootloader = *bootloader_ptr;

  // jump to bootloader
  enable_interrupts();
  bootloader();

  // reset on exit
  enter_bootloader_mode = BOOT_NORMAL;
  NVIC_SystemReset();
}

void early_initialization(void) {
  // Reset global critical depth
  disable_interrupts();
  global_critical_depth = 0;

  // Init register and interrupt tables
  init_registers();

  // after it's been in the bootloader, things are initted differently, so we reset
  if ((enter_bootloader_mode != BOOT_NORMAL) &&
      (enter_bootloader_mode != ENTER_BOOTLOADER_MAGIC) &&
      (enter_bootloader_mode != ENTER_SOFTLOADER_MAGIC)) {
    enter_bootloader_mode = BOOT_NORMAL;
    NVIC_SystemReset();
  }

  // if wrong chip, reboot
  volatile unsigned int id = DBGMCU->IDCODE;
  if ((id & 0xFFFU) != MCU_IDCODE) {
    enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
  }

  // setup interrupt table
  SCB->VTOR = (uint32_t)&g_pfnVectors;

  // early GPIOs float everything
  early_gpio_float();

  detect_board_type();

  if (enter_bootloader_mode == ENTER_BOOTLOADER_MAGIC) {
    led_init();
    current_board->init_bootloader();
    led_set(LED_GREEN, 1);
    jump_to_bootloader();
  }
}
