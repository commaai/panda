#ifdef STM32F4
  #define PANDA
  #include "stm32f4xx.h"
#else
  #include "stm32f2xx.h"
#endif

#include "early.h"
#include "libc.h"

#include "crypto/rsa.h"
#include "crypto/sha.h"

#include "obj/cert.h"

void __initialize_hardware_early() {
  early();
}

void fail() {
  enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
  NVIC_SystemReset();
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
  if (!RSA_verify(&rsa_key, ((void*)&_app_start[0]) + len, RSANUMBYTES, digest, SHA_DIGEST_SIZE)) {
    fail();
  }

  // jump to flash
  ((void(*)()) _app_start[1])();
  return 0;
}

