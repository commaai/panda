#include "board/config.h"
#include "board/early_init.h"
#include "board/drivers/led.h"

static void jump_to_bootloader(void) {
  enter_bootloader_mode = 0;
  bootloader_fcn_ptr bootloader_ptr = (bootloader_fcn_ptr)BOOTLOADER_ADDRESS;
  bootloader_fcn bootloader = *bootloader_ptr;
  enable_interrupts();
  bootloader();
  enter_bootloader_mode = BOOT_NORMAL;
  NVIC_SystemReset();
}

void early_initialization(void) {
  disable_interrupts();
  global_critical_depth = 0;
  init_registers();
  if ((enter_bootloader_mode != BOOT_NORMAL) &&
      (enter_bootloader_mode != ENTER_BOOTLOADER_MAGIC) &&
      (enter_bootloader_mode != ENTER_SOFTLOADER_MAGIC)) {
    enter_bootloader_mode = BOOT_NORMAL;
    NVIC_SystemReset();
  }
  volatile unsigned int id = DBGMCU->IDCODE;
  if ((id & 0xFFFU) != MCU_IDCODE) {
    enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
  }
  SCB->VTOR = (uint32_t)&g_pfnVectors;
  early_gpio_float();
  detect_board_type();
  if (enter_bootloader_mode == ENTER_BOOTLOADER_MAGIC) {
    led_init();
    current_board->init_bootloader();
    led_set(LED_GREEN, 1);
    jump_to_bootloader();
  }
}
