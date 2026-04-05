#ifndef PANDA_DRIVERS_FAKE_SIREN_H
#define PANDA_DRIVERS_FAKE_SIREN_H

#include <stdbool.h>

void fake_i2c_siren_set(bool enabled);
void fake_siren_set(bool enabled);

#endif
