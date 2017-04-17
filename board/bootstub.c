#ifdef STM32F4
  #define PANDA
  #include "stm32f4xx.h"
#else
  #include "stm32f2xx.h"
#endif

#include "early.h"
#include "libc.h"

void __initialize_hardware_early() {
  early();
}

int main() {
  clock_init();

  // TODO: do signature check

  // jump to flash
  ((void(*)()) _app_start[1])();
  return 0;
}

