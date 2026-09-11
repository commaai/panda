#pragma once

#include "board/drivers/register_declarations.h"

// One tracking object per hardware register, shared by every writer.
// Keep command/status registers out of this inventory.
// cppcheck-suppress-begin misra-c2012-20.5 ; Undefine private X-macro helpers after constructing the inventory.
// cppcheck-suppress-begin misra-c2012-20.7 ; X-macro parameters include member names and macro names, not just expressions.

#define GPIO_REGISTERS(X, p, h) \
  X(p, h, AFR0, AFR[0]) \
  X(p, h, AFR1, AFR[1]) \
  X(p, h, MODER, MODER) \
  X(p, h, ODR, ODR) \
  X(p, h, OSPEEDR, OSPEEDR) \
  X(p, h, OTYPER, OTYPER) \
  X(p, h, PUPDR, PUPDR)

#define GPIO_INSTANCES(X) \
  X(GPIOA) \
  X(GPIOB) \
  X(GPIOC) \
  X(GPIOD) \
  X(GPIOE) \
  X(GPIOF) \
  X(GPIOG) \
  X(GPIOH)

#define REGISTER_FIELD(p, h, name, field) tracked_register name;
#define REGISTER_STATE_FIELD(p, h, name, field) tracked_register_state name;
typedef struct {
  GPIO_REGISTERS(REGISTER_STATE_FIELD, unused, unused)
} tracked_gpio_state;
#undef REGISTER_STATE_FIELD

struct tracked_gpio {
  GPIO_TypeDef * const hardware;
  GPIO_REGISTERS(REGISTER_FIELD, unused, unused)
};
#undef REGISTER_FIELD

#define REGISTER_INIT(p, h, name, field) .name = {.address = &((h)->field), .state = &((p).name)},
#define DEFINE_GPIO(p) static tracked_gpio_state state_##p; \
  static const tracked_gpio tracked_##p = {.hardware = (p), GPIO_REGISTERS(REGISTER_INIT, state_##p, p)};
GPIO_INSTANCES(DEFINE_GPIO)
#undef DEFINE_GPIO
#undef REGISTER_INIT

#define TIM_REGISTERS(X, p, h) \
  X(p, h, AF1, AF1) \
  X(p, h, ARR, ARR) \
  X(p, h, BDTR, BDTR) \
  X(p, h, CCER, CCER) \
  X(p, h, CCMR1, CCMR1) \
  X(p, h, CCMR2, CCMR2) \
  X(p, h, CCR1, CCR1) \
  X(p, h, CCR2, CCR2) \
  X(p, h, CCR3, CCR3) \
  X(p, h, CCR4, CCR4) \
  X(p, h, CR1, CR1) \
  X(p, h, CR2, CR2) \
  X(p, h, DIER, DIER) \
  X(p, h, PSC, PSC) \
  X(p, h, SMCR, SMCR)

#define TIM_INSTANCES(X) \
  X(TIM1) \
  X(TIM12) \
  X(TIM3) \
  X(TIM5) \
  X(TIM6) \
  X(TIM7) \
  X(TIM8)

#define REGISTER_FIELD(p, h, name, field) tracked_register name;
#define REGISTER_STATE_FIELD(p, h, name, field) tracked_register_state name;
typedef struct {
  TIM_REGISTERS(REGISTER_STATE_FIELD, unused, unused)
} tracked_timer_state;
#undef REGISTER_STATE_FIELD

struct tracked_timer {
  TIM_TypeDef * const hardware;
  TIM_REGISTERS(REGISTER_FIELD, unused, unused)
};
#undef REGISTER_FIELD

#define REGISTER_INIT(p, h, name, field) .name = {.address = &((h)->field), .state = &((p).name)},
#define DEFINE_TIM(p) static tracked_timer_state state_##p; \
  static const tracked_timer tracked_##p = {.hardware = (p), TIM_REGISTERS(REGISTER_INIT, state_##p, p)};
// cppcheck-suppress misra-c2012-8.9 ; Objects need static ownership even when this variant only references them through the checker.
TIM_INSTANCES(DEFINE_TIM)
#undef DEFINE_TIM
#undef REGISTER_INIT

#define I2C_REGISTERS(X, p, h) \
  X(p, h, CR1, CR1) \
  X(p, h, CR2, CR2)

#define I2C_INSTANCES(X) \
  X(I2C5)

#define REGISTER_FIELD(p, h, name, field) tracked_register name;
#define REGISTER_STATE_FIELD(p, h, name, field) tracked_register_state name;
typedef struct {
  I2C_REGISTERS(REGISTER_STATE_FIELD, unused, unused)
} tracked_i2c_state;
#undef REGISTER_STATE_FIELD

struct tracked_i2c {
  I2C_TypeDef * const hardware;
  I2C_REGISTERS(REGISTER_FIELD, unused, unused)
};
#undef REGISTER_FIELD

#define REGISTER_INIT(p, h, name, field) .name = {.address = &((h)->field), .state = &((p).name)},
#define DEFINE_I2C(p) static tracked_i2c_state state_##p; \
  static const tracked_i2c tracked_##p = {.hardware = (p), I2C_REGISTERS(REGISTER_INIT, state_##p, p)};
I2C_INSTANCES(DEFINE_I2C)
#undef DEFINE_I2C
#undef REGISTER_INIT

#define OTHER_REGISTERS(X) \
  X(BDMA_Channel0_CCR, &(BDMA_Channel0->CCR)) \
  X(BDMA_Channel0_CM0AR, &(BDMA_Channel0->CM0AR)) \
  X(BDMA_Channel0_CM1AR, &(BDMA_Channel0->CM1AR)) \
  X(BDMA_Channel0_CPAR, &(BDMA_Channel0->CPAR)) \
  X(BDMA_Channel1_CCR, &(BDMA_Channel1->CCR)) \
  X(BDMA_Channel1_CM0AR, &(BDMA_Channel1->CM0AR)) \
  X(BDMA_Channel1_CM1AR, &(BDMA_Channel1->CM1AR)) \
  X(BDMA_Channel1_CPAR, &(BDMA_Channel1->CPAR)) \
  X(DAC1_CR, &(DAC1->CR)) \
  X(DAC1_MCR, &(DAC1->MCR)) \
  X(DFSDM1_Channel0_CHCFGR1, &(DFSDM1_Channel0->CHCFGR1)) \
  X(DFSDM1_Channel3_CHCFGR1, &(DFSDM1_Channel3->CHCFGR1)) \
  X(DFSDM1_Filter0_FLTCR1, &(DFSDM1_Filter0->FLTCR1)) \
  X(DFSDM1_Filter0_FLTFCR, &(DFSDM1_Filter0->FLTFCR)) \
  X(DMA1_Stream0_CR, &(DMA1_Stream0->CR)) \
  X(DMA1_Stream0_M0AR, &(DMA1_Stream0->M0AR)) \
  X(DMA1_Stream0_M1AR, &(DMA1_Stream0->M1AR)) \
  X(DMA1_Stream0_PAR, &(DMA1_Stream0->PAR)) \
  X(DMA1_Stream1_CR, &(DMA1_Stream1->CR)) \
  X(DMA1_Stream1_FCR, &(DMA1_Stream1->FCR)) \
  X(DMA1_Stream1_M0AR, &(DMA1_Stream1->M0AR)) \
  X(DMA1_Stream1_M1AR, &(DMA1_Stream1->M1AR)) \
  X(DMA1_Stream1_PAR, &(DMA1_Stream1->PAR)) \
  X(DMA2_Stream2_CR, &(DMA2_Stream2->CR)) \
  X(DMA2_Stream2_M0AR, &(DMA2_Stream2->M0AR)) \
  X(DMA2_Stream2_PAR, &(DMA2_Stream2->PAR)) \
  X(DMA2_Stream3_CR, &(DMA2_Stream3->CR)) \
  X(DMA2_Stream3_M0AR, &(DMA2_Stream3->M0AR)) \
  X(DMA2_Stream3_PAR, &(DMA2_Stream3->PAR)) \
  X(DMAMUX1_Channel0_CCR, &(DMAMUX1_Channel0->CCR)) \
  X(DMAMUX1_Channel1_CCR, &(DMAMUX1_Channel1->CCR)) \
  X(DMAMUX1_Channel10_CCR, &(DMAMUX1_Channel10->CCR)) \
  X(DMAMUX1_Channel11_CCR, &(DMAMUX1_Channel11->CCR)) \
  X(DMAMUX2_Channel0_CCR, &(DMAMUX2_Channel0->CCR)) \
  X(DMAMUX2_Channel1_CCR, &(DMAMUX2_Channel1->CCR)) \
  X(DTS_CFGR1, &(DTS->CFGR1)) \
  X(EXTI_FTSR1, &(EXTI->FTSR1)) \
  X(EXTI_IMR1, &(EXTI->IMR1)) \
  X(EXTI_RTSR1, &(EXTI->RTSR1)) \
  X(FLASH_ACR, &(FLASH->ACR)) \
  X(PWR_CPUCR, &(PWR->CPUCR)) \
  X(PWR_CR1, &(PWR->CR1)) \
  X(PWR_CR3, &(PWR->CR3)) \
  X(PWR_D3CR, &(PWR->D3CR)) \
  X(RCC_AHB2LPENR, &(RCC->AHB2LPENR)) \
  X(RCC_AHB3LPENR, &(RCC->AHB3LPENR)) \
  X(RCC_AHB4LPENR, &(RCC->AHB4LPENR)) \
  X(RCC_APB1LENR, &(RCC->APB1LENR)) \
  X(RCC_CFGR, &(RCC->CFGR)) \
  X(RCC_CR, &(RCC->CR)) \
  X(RCC_D1CFGR, &(RCC->D1CFGR)) \
  X(RCC_D2CCIP1R, &(RCC->D2CCIP1R)) \
  X(RCC_D2CCIP2R, &(RCC->D2CCIP2R)) \
  X(RCC_D2CFGR, &(RCC->D2CFGR)) \
  X(RCC_D3CCIPR, &(RCC->D3CCIPR)) \
  X(RCC_D3CFGR, &(RCC->D3CFGR)) \
  X(RCC_PLL1DIVR, &(RCC->PLL1DIVR)) \
  X(RCC_PLLCFGR, &(RCC->PLLCFGR)) \
  X(RCC_PLLCKSELR, &(RCC->PLLCKSELR)) \
  X(SAI4_GCR, &(SAI4->GCR)) \
  X(SAI4_Block_A_CR1, &(SAI4_Block_A->CR1)) \
  X(SAI4_Block_A_CR2, &(SAI4_Block_A->CR2)) \
  X(SAI4_Block_A_FRCR, &(SAI4_Block_A->FRCR)) \
  X(SAI4_Block_A_SLOTR, &(SAI4_Block_A->SLOTR)) \
  X(SAI4_Block_B_CR1, &(SAI4_Block_B->CR1)) \
  X(SAI4_Block_B_CR2, &(SAI4_Block_B->CR2)) \
  X(SAI4_Block_B_FRCR, &(SAI4_Block_B->FRCR)) \
  X(SAI4_Block_B_SLOTR, &(SAI4_Block_B->SLOTR)) \
  X(SPI4_CFG1, &(SPI4->CFG1)) \
  X(SPI4_CR1, &(SPI4->CR1)) \
  X(SPI4_CR2, &(SPI4->CR2)) \
  X(SPI4_IER, &(SPI4->IER)) \
  X(SPI4_UDRDR, &(SPI4->UDRDR)) \
  X(SYSCFG_EXTICR_0, &(SYSCFG->EXTICR[0])) \
  X(SYSCFG_EXTICR_1, &(SYSCFG->EXTICR[1])) \
  X(SYSCFG_EXTICR_2, &(SYSCFG->EXTICR[2])) \
  X(SYSCFG_EXTICR_3, &(SYSCFG->EXTICR[3])) \
  X(SYSCFG_PMCR, &(SYSCFG->PMCR))

#define DEFINE_REGISTER(name, addr) static tracked_register_state state_##name; \
  static const tracked_register tracked_##name = {.address = (addr), .state = &state_##name};
// cppcheck-suppress misra-c2012-8.9 ; Objects are shared across drivers and firmware variants.
OTHER_REGISTERS(DEFINE_REGISTER)
#undef DEFINE_REGISTER

#define REGISTER_REF(p, h, name, field) &((p).name),
#define OTHER_REGISTER_REF(name, addr) &tracked_##name,
#define GPIO_REFS(p) GPIO_REGISTERS(REGISTER_REF, tracked_##p, unused)
#define TIM_REFS(p) TIM_REGISTERS(REGISTER_REF, tracked_##p, unused)
#define I2C_REFS(p) I2C_REGISTERS(REGISTER_REF, tracked_##p, unused)
static const tracked_register * const tracked_registers[] = {
  GPIO_INSTANCES(GPIO_REFS)
  TIM_INSTANCES(TIM_REFS)
  I2C_INSTANCES(I2C_REFS)
  OTHER_REGISTERS(OTHER_REGISTER_REF)
};
#undef REGISTER_REF
#undef OTHER_REGISTER_REF
#undef GPIO_REFS
#undef GPIO_REGISTERS
#undef GPIO_INSTANCES
#undef TIM_REFS
#undef TIM_REGISTERS
#undef TIM_INSTANCES
#undef I2C_REFS
#undef I2C_REGISTERS
#undef I2C_INSTANCES
#undef OTHER_REGISTERS

// cppcheck-suppress-end misra-c2012-20.7
// cppcheck-suppress-end misra-c2012-20.5
