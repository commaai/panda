#ifndef CLOCK_H
#define CLOCK_H

#include "board/config.h"

typedef enum {
  PACKAGE_UNKNOWN = 0,
  PACKAGE_WITH_SMPS = 1,
  PACKAGE_WITHOUT_SMPS = 2,
} PackageSMPSType;

PackageSMPSType get_package_smps_type(void);
void clock_init(void);

#endif
