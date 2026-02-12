#pragma once

#include <stdint.h>

// ************************ High-level debug functions **********************
void putch(const char a);
void print(const char *a);
void puthx(uint32_t i, uint8_t len);
void puth(unsigned int i);
#if defined(DEBUG_SPI) || defined(BOOTSTUB) || defined(DEBUG)
void puth4(unsigned int i);
#endif
#if defined(DEBUG_SPI) || defined(DEBUG_USB) || defined(DEBUG_COMMS)
void hexdump(const void *a, int l);
#endif