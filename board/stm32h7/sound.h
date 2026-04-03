#ifndef PANDA_STM32H7_SOUND_H
#define PANDA_STM32H7_SOUND_H

#include <stdint.h>

void sound_tick(void);
void sound_init_dac(void);
void sound_stop_dac(void);
void sound_init(void);

#endif
