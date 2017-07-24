#define BOOTSTUB

#ifdef STM32F4
  #define PANDA
  #include "stm32f4xx.h"
  #include "stm32f4xx_hal_gpio_ex.h"
#else
  #include "stm32f2xx.h"
  #include "stm32f2xx_hal_gpio_ex.h"
#endif

#include "libc.h"
#include "gpio.h"

#include "drivers/drivers.h"
#include "drivers/spi.h"

#include "crypto/rsa.h"
#include "crypto/sha.h"

#include "obj/cert.h"

#include "spi_flasher.h"

void __initialize_hardware_early() {
  early();

  if (is_entering_bootmode) {
    spi_flasher();
  }
}

void fail() {
#ifdef PANDA
  // detect usb host
  int no_usb = detect_with_pull(GPIOA, 11, PULL_UP);

  if (no_usb) {
    // no usb host, go to SPI flasher
    spi_flasher();
  }
#else
  enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
  NVIC_SystemReset();
#endif
}

// know where to sig check
extern void *_app_start[];

int main() {
  clock_init();

  // validate length
  int len = (int)_app_start[0];
  if ((len < 8) || (((uint32_t)&_app_start[0] + RSANUMBYTES) >= 0x8100000)) fail();

  // compute SHA hash
  uint8_t digest[SHA_DIGEST_SIZE];
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

