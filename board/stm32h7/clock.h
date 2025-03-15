#pragma once

/*
HSE: 25MHz
PLL1Q: 80MHz (for FDCAN)
HSI48 enabled (for USB)
CPU: 240MHz
CPU Systick: 240MHz
AXI: 120MHz
HCLK3: 60MHz
APB3 per: 60MHz
AHB1,2 per: 120MHz
APB1 per: 60MHz
APB1 tim: 120MHz
APB2 per: 60MHz
APB2 tim: 120MHz
AHB4 per: 120MHz
APB4 per: 60MHz
PCLK1: 60MHz (for USART2,3,4,5,7,8)
*/

typedef enum {
  PACKAGE_UNKNOWN = 0,
  PACKAGE_WITH_SMPS = 1,
  PACKAGE_WITHOUT_SMPS = 2,
} PackageSMPSType;

void clock_init(void);
