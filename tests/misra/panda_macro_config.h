// This is a dummy structure to allow cppcheck to test certain macro combinations 
// together and in one seesion (needed for things like unused macros) 
// The structure should represents macro configurations used in scons

#ifdef STM32H7
  #ifdef STM32H725xx
  #elif defined(STM32H723)
  #else
  #endif
#elif defined(STM32F4)
  #ifdef STM32F413xx
    #ifdef ENABLE_SPI
    #endif
  #endif
#else
#endif
