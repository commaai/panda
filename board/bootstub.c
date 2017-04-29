#define BOOTSTUB

#ifdef STM32F4
  #define PANDA
  #include "stm32f4xx.h"
  #include "stm32f4xx_hal_gpio_ex.h"
#else
  #include "stm32f2xx.h"
  #include "stm32f2xx_hal_gpio_ex.h"
#endif

#include "early.h"
#include "libc.h"
#include "spi.h"

#include "crypto/rsa.h"
#include "crypto/sha.h"

#include "obj/cert.h"

#include "spi_flasher.h"

void __initialize_hardware_early() {
  early();
}

void fail() {
#ifdef PANDA
  spi_flasher();
#else
  enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
  NVIC_SystemReset();
#endif
}

int main() {
  clock_init();

  // validate length
  int len = _app_start[0];
  if (len < 8) fail();

  // compute SHA hash
  char digest[SHA_DIGEST_SIZE];
  SHA_hash(&_app_start[1], len-4, digest);

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
  fail();
good:
  // jump to flash
  ((void(*)()) _app_start[1])();
  return 0;
}

