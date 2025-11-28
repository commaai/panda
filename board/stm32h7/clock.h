#pragma once

typedef enum {
  PACKAGE_UNKNOWN = 0,
  PACKAGE_WITH_SMPS = 1,
  PACKAGE_WITHOUT_SMPS = 2,
} PackageSMPSType;

// TODO: find a better way to distinguish between H725 (using SMPS) and H723 (lacking SMPS)
// The package will do for now, since we have only used TFBGA100 for H723

void clock_init(void);