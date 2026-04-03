#include "board/config.h"
#include "board/utils.h"
#include "board/stm32h7/clock.h"

PackageSMPSType get_package_smps_type(void) {
  PackageSMPSType ret;
  RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;
  switch(SYSCFG->PKGR & 0xFU) {
    case 0b0001U:
    case 0b0011U:
      ret = PACKAGE_WITHOUT_SMPS;
      break;
    case 0b0101U:
    case 0b0111U:
    case 0b1000U:
      ret = PACKAGE_WITH_SMPS;
      break;
    default:
      ret = PACKAGE_UNKNOWN;
  }
  return ret;
}

void clock_init(void) {
  PackageSMPSType package_smps = get_package_smps_type();
  if (package_smps == PACKAGE_WITHOUT_SMPS) {
    register_set(&(PWR->CR3), PWR_CR3_LDOEN, 0xFU);
  } else if (package_smps == PACKAGE_WITH_SMPS) {
    register_set(&(PWR->CR3), PWR_CR3_SMPSEN, 0xFU);
  } else {
    while(true);
  }

  register_set(&(PWR->D3CR), PWR_D3CR_VOS_1 | PWR_D3CR_VOS_0, 0xC000U);
  while ((PWR->CSR1 & PWR_CSR1_ACTVOSRDY) == 0U);
  while ((PWR->CSR1 & PWR_CSR1_ACTVOS) != (PWR->D3CR & PWR_D3CR_VOS));

  register_set(&(FLASH->ACR), FLASH_ACR_LATENCY_2WS | 0x20U, 0x3FU);
  register_set_bits(&(RCC->CR), RCC_CR_HSEON);
  while ((RCC->CR & RCC_CR_HSERDY) == 0U);
  register_set_bits(&(RCC->CR), RCC_CR_HSI48ON);
  while ((RCC->CR & RCC_CR_HSI48RDY) == 0U);
  register_set(&(RCC->PLLCKSELR), RCC_PLLCKSELR_PLLSRC_HSE | RCC_PLLCKSELR_DIVM1_0 | RCC_PLLCKSELR_DIVM1_2 | RCC_PLLCKSELR_DIVM2_0 | RCC_PLLCKSELR_DIVM2_2 | RCC_PLLCKSELR_DIVM3_0 | RCC_PLLCKSELR_DIVM3_2, 0x3F3F3F3U);
  register_set(&(RCC->PLL1DIVR), 0x102002FU, 0x7F7FFFFFU);
  register_set(&(RCC->PLLCFGR), RCC_PLLCFGR_PLL1RGE_2 | RCC_PLLCFGR_DIVP1EN | RCC_PLLCFGR_DIVQ1EN | RCC_PLLCFGR_DIVR1EN, 0x7000CU);
  register_set_bits(&(RCC->CR), RCC_CR_PLL1ON);
  while((RCC->CR & RCC_CR_PLL1RDY) == 0U);
  register_set(&(RCC->D1CFGR), RCC_D1CFGR_HPRE_DIV2 | RCC_D1CFGR_D1PPRE_DIV2 | RCC_D1CFGR_D1CPRE_DIV1, 0xF7FU);
  register_set(&(RCC->D2CFGR), RCC_D2CFGR_D2PPRE1_DIV2 | RCC_D2CFGR_D2PPRE2_DIV2, 0x770U);
  register_set(&(RCC->D3CFGR), RCC_D3CFGR_D3PPRE_DIV2, 0x70U);
  register_set(&(RCC->CFGR), RCC_CFGR_SW_PLL1, 0x7U);
  while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL1);
  register_set(&(RCC->D2CCIP2R), RCC_D2CCIP2R_USBSEL_1 | RCC_D2CCIP2R_USBSEL_0, RCC_D2CCIP2R_USBSEL);
  register_set(&(RCC->D2CCIP1R), RCC_D2CCIP1R_FDCANSEL_0, RCC_D2CCIP1R_FDCANSEL);
  register_set_bits(&(RCC->D2CCIP1R), RCC_D2CCIP1R_DFSDM1SEL);
  register_set(&(RCC->D3CCIPR), RCC_D3CCIPR_ADCSEL_1, RCC_D3CCIPR_ADCSEL);
  register_set_bits(&(RCC->CR), RCC_CR_CSSHSEON);
  register_set_bits(&(PWR->CR3), PWR_CR3_USB33DEN);
}
