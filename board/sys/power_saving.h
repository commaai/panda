#ifndef POWER_SAVING_H
#define POWER_SAVING_H

#include "board/sys/sys.h"

extern bool power_save_enabled;
#ifdef ALLOW_DEBUG
extern volatile bool stop_mode_requested;
#endif

void enable_can_transceivers(bool enabled);
void set_power_save_state(bool enable);
void enter_stop_mode(void);

#endif
