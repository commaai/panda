#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "stm32h7xx.h"
#include "stm32h7xx_hal_gpio_ex.h"
#include "board/drivers/drivers.h"
#include "board/provision.h"
#include "board/comms_definitions.h"
#include "board/libc.h"
#include "board/utils.h"
#include "board/stm32h7/llflash.h"

// Forward declarations for functions defined in headers included by bootstub.c
#define LED_RED 0U
#define LED_GREEN 1U
#define LED_BLUE 2U
void led_init(void);
void led_set(uint8_t color, bool enabled);
void flasher_peripherals_init(void);
void gpio_usart2_init(void);
void gpio_usb_init(void);
void gpio_spi_init(void);
void enable_interrupts(void);

// gitversion - definition is in bootstub.c via gitversion.h
// Only include the extern declaration, not the definition
extern const uint8_t gitversion[];

// from main/bootstub
extern uint8_t hw_type;
#ifdef PANDA_JUNGLE
  #include "board/jungle/boards/board_declarations.h"
#elif defined(PANDA_BODY)
  #include "board/body/boards/board_declarations.h"
#else
  #include "board/boards/board_declarations.h"
#endif
extern struct board *current_board;

// Constants from config.h / stm32h7_config.h
#define USBPACKET_MAX_SIZE 0x40U
#define DEVICE_SERIAL_NUMBER_ADDRESS 0x080FFFC0U

// from early_init
#define ENTER_BOOTLOADER_MAGIC 0xdeadbeefU
#define ENTER_SOFTLOADER_MAGIC 0xdeadc0deU
extern uint32_t enter_bootloader_mode;

// from the linker script
#define APP_START_ADDRESS 0x8020000U

// flasher state variables
uint32_t *prog_ptr = NULL;
bool unlocked = false;

int comms_control_handler(ControlPacket_t *req, uint8_t *resp) {
  int resp_len = 0;

  // flasher machine
  memset(resp, 0, 4);
  memcpy(resp+4, "\xde\xad\xd0\x0d", 4);
  resp[0] = 0xff;
  resp[2] = req->request;
  resp[3] = ~req->request;
  *((uint32_t **)&resp[8]) = prog_ptr;
  resp_len = 0xc;

  int sec;
  switch (req->request) {
    // **** 0xb0: flasher echo
    case 0xb0:
      resp[1] = 0xff;
      break;
    // **** 0xb1: unlock flash
    case 0xb1:
      if (flash_is_locked()) {
        flash_unlock();
        resp[1] = 0xff;
      }
      led_set(LED_GREEN, 1);
      unlocked = true;
      prog_ptr = (uint32_t *)APP_START_ADDRESS;
      break;
    // **** 0xb2: erase sector
    case 0xb2:
      sec = req->param1;
      if (flash_erase_sector(sec, unlocked)) {
        resp[1] = 0xff;
      }
      break;
    // **** 0xc1: get hardware type
    case 0xc1:
      resp[0] = hw_type;
      resp_len = 1;
      break;
    // **** 0xc3: fetch MCU UID
    case 0xc3:
      (void)memcpy(resp, ((uint8_t *)UID_BASE), 12);
      resp_len = 12;
      break;
    // **** 0xd0: fetch serial number
    case 0xd0:
      // addresses are OTP
      if (req->param1 == 1) {
        memcpy(resp, (void *)DEVICE_SERIAL_NUMBER_ADDRESS, 0x10);
        resp_len = 0x10;
      } else {
        get_provision_chunk(resp);
        resp_len = PROVISION_CHUNK_LEN;
      }
      break;
    // **** 0xd1: enter bootloader mode
    case 0xd1:
      // this allows reflashing of the bootstub
      switch (req->param1) {
        case 0:
          print("-> entering bootloader\n");
          enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
          NVIC_SystemReset();
          break;
        case 1:
          print("-> entering softloader\n");
          enter_bootloader_mode = ENTER_SOFTLOADER_MAGIC;
          NVIC_SystemReset();
          break;
      }
      break;
    // **** 0xd6: get version
    case 0xd6:
      {
        // gitversion is a null-terminated string defined in bootstub.c
        uint32_t git_len = 0;
        while (gitversion[git_len] != 0U) { git_len++; }
        git_len++; // include null terminator
        memcpy(resp, gitversion, git_len);
        resp_len = git_len - 1U;
      }
      break;
    // **** 0xd8: reset ST
    case 0xd8:
      flush_write_buffer();
      NVIC_SystemReset();
      break;
  }
  return resp_len;
}

void comms_can_write(const uint8_t *data, uint32_t len) {
  UNUSED(data);
  UNUSED(len);
}

int comms_can_read(uint8_t *data, uint32_t max_len) {
  UNUSED(data);
  UNUSED(max_len);
  return 0;
}

void refresh_can_tx_slots_available(void) {}

void comms_endpoint2_write(const uint8_t *data, uint32_t len) {
  led_set(LED_RED, 0);
  for (uint32_t i = 0; i < len/4; i++) {
    flash_write_word(prog_ptr, *(uint32_t*)(data+(i*4)));

    //*(uint64_t*)(&spi_tx_buf[0x30+(i*4)]) = *prog_ptr;
    prog_ptr++;
  }
  led_set(LED_RED, 1);
}


void soft_flasher_start(void) {
  print("\n\n\n************************ FLASHER START ************************\n");

  enter_bootloader_mode = 0;

  flasher_peripherals_init();

  gpio_usart2_init();
  gpio_usb_init();
  led_init();

  // enable comms
  usb_init();
  if (current_board->has_spi) {
    gpio_spi_init();
    spi_init();
  }

  // green LED on for flashing
  led_set(LED_GREEN, 1);

  enable_interrupts();

  for (;;) {
    // blink the green LED fast
    led_set(LED_GREEN, 0);
    delay(500000);
    led_set(LED_GREEN, 1);
    delay(500000);
  }
}
