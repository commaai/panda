void *_origin = 0x8004000;

#ifdef STM32F4
  #define PANDA
  #include "stm32f4xx.h"
#else
  #include "stm32f2xx.h"
#endif

#include "early.h"

void __initialize_hardware_early() {
  early();

  // jump to flash
  ((void(*)()) _app_start[1])();
}

int main() {
  return 0;
}

