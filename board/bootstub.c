#define VERS_TAG 0x53524556
#define MIN_VERSION 2

// ********************* Includes *********************
#include <stdbool.h>

#include "board/config.h"
#include "board/early_init.h"
#include "board/flasher.h"

#include "crypto/rsa.h"
#include "crypto/sha.h"

#include "board/obj/cert.h"
#include "board/print.h"
#include "board/drivers/uart.h"

#include "globals.h"

// cppcheck-suppress unusedFunction ; used in headers not included in cppcheck
void __initialize_hardware_early(void) {
  early_initialization();
}

void debug_ring_callback(uart_ring *ring) {
  char rcv;
  while (get_char(ring, &rcv)) { } // Intentionally disabled, no uart in bootstub
}

static void fail(void) {
  soft_flasher_start();
}

int main(void) {
  // Init interrupt table
  init_interrupts(true);

  disable_interrupts();
  clock_init();
  detect_board_type();

#ifdef PANDA_JUNGLE
  current_board->set_panda_power(true);
#endif

  if (enter_bootloader_mode == ENTER_SOFTLOADER_MAGIC) {
    enter_bootloader_mode = 0;
    soft_flasher_start();
  }

  // validate length
  int len = (int)_app_start[0];
  if ((len < 8) || (len > (0x1000000 - 0x4000 - 4 - RSANUMBYTES))) goto fail;

  // compute SHA hash
  uint8_t digest[SHA_DIGEST_SIZE];
  SHA_hash(&_app_start[1], len-4, digest);

  // verify version, last bytes in the signed area
  uint32_t vers[2] = {0};
  (void)memcpy(&vers, ((void*)&_app_start[0]) + len - sizeof(vers), sizeof(vers));
  if (vers[0] != VERS_TAG || vers[1] < MIN_VERSION) {
    goto fail;
  }

  // verify RSA signature
  if (RSA_verify(&release_rsa_key, ((void*)&_app_start[0]) + len, RSANUMBYTES, digest, SHA_DIGEST_SIZE)) {
    goto good;
  }

  // allow debug if built from source
#ifdef ALLOW_DEBUG
  if (RSA_verify(&debug_rsa_key, ((void*)&_app_start[0]) + len, RSANUMBYTES, digest, SHA_DIGEST_SIZE)) {
    goto good;
  }
#endif

// here is a failure
fail:
  fail();
  return 0;
good:
  // jump to flash
  ((void(*)(void)) _app_start[1])();
  return 0;
}
