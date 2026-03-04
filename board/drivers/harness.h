#ifndef DRIVERS_HARNESS_H
#define DRIVERS_HARNESS_H

#include <stdbool.h>

void set_intercept_relay(bool intercept, bool ignition_relay);
bool harness_check_ignition(void);
void harness_tick(void);
void harness_init(void);

#endif
