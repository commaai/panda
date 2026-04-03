#include "board/config.h"
#include "board/sys/power_saving.h"
#include "board/drivers/harness.h"
#include "board/drivers/fdcan.h"
#include "board/drivers/bootkick.h"

bool power_save_enabled = false;
#ifdef ALLOW_DEBUG
volatile bool stop_mode_requested = false;
#endif

void enable_can_transceivers(bool enabled) {
  uint8_t main_bus = (harness.status == HARNESS_STATUS_FLIPPED) ? 3U : 1U;
  for(uint8_t i=1U; i<=4U; i++){
    current_board->enable_can_transceiver(i, (i == main_bus) || enabled);
  }
}

void set_power_save_state(bool enable) {
  if (enable != power_save_enabled) {
    if (enable) {
      print("enable power savings\n");
      if (harness.status == HARNESS_STATUS_FLIPPED) { llcan_irq_disable(cans[0]); }
      else { llcan_irq_disable(cans[2]); }
      llcan_irq_disable(cans[1]);
    } else {
      print("disable power savings\n");
      if (harness.status == HARNESS_STATUS_FLIPPED) { llcan_irq_enable(cans[0]); }
      else { llcan_irq_enable(cans[2]); }
      llcan_irq_enable(cans[1]);
    }
    enable_can_transceivers(!enable);
    if(enable){ current_board->set_ir_power(0U); }
    power_save_enabled = enable;
  }
}

void enter_stop_mode(void) {
  register_set(&(GPIOA->MODER), 0xFFFFFFFFU, 0xFFFFFFFFU);
  register_set(&(GPIOB->MODER), 0xFFFFFFFFU, 0xFFFFFFFFU);
  register_set(&(GPIOC->MODER), 0xFFFFFFFFU, 0xFFFFFFFFU);
  register_set(&(GPIOD->MODER), 0xFFFFFFFFU, 0xFFFFFFFFU);
  register_set(&(GPIOE->MODER), 0xFFFFFFFFU, 0xFFFFFFFFU);
  register_set(&(GPIOF->MODER), 0xFFFFFFFFU, 0xFFFFFFFFU);
  register_set(&(GPIOG->MODER), 0xFFFFFFFFU, 0xFFFFFFFFU);
  current_board->set_bootkick(BOOT_STANDBY);
  current_board->set_amp_enabled(false);
  for (uint8_t i = 1U; i <= 4U; i++) { current_board->enable_can_transceiver(i, false); }
  ADC1->CR &= ~(ADC_CR_ADEN); ADC1->CR |= ADC_CR_DEEPPWD;
  ADC2->CR &= ~(ADC_CR_ADEN); ADC2->CR |= ADC_CR_DEEPPWD;
  register_clear_bits(&(RCC->CR), RCC_CR_HSI48ON);
  register_clear_bits(&(RCC->AHB2LPENR), RCC_AHB2LPENR_SRAM1LPEN | RCC_AHB2LPENR_SRAM2LPEN);
  register_clear_bits(&(RCC->AHB4LPENR), RCC_AHB4LPENR_SRAM4LPEN);
  register_clear_bits(&(RCC->AHB3LPENR), RCC_AHB3LPENR_AXISRAMLPEN);
  set_gpio_mode(current_board->harness_config->GPIO_SBU1, current_board->harness_config->pin_SBU1, MODE_INPUT);
  set_gpio_mode(current_board->harness_config->GPIO_SBU2, current_board->harness_config->pin_SBU2, MODE_INPUT);
  register_set(&(SYSCFG->EXTICR[0]), SYSCFG_EXTICR1_EXTI1_PA, 0xF0U);
  register_set(&(SYSCFG->EXTICR[1]), SYSCFG_EXTICR2_EXTI4_PC, 0xFU);
  register_set_bits(&(EXTI->IMR1), (1U << 1) | (1U << 4));
  register_set_bits(&(EXTI->RTSR1), (1U << 1) | (1U << 4));
  register_set_bits(&(EXTI->FTSR1), (1U << 1) | (1U << 4));
  set_gpio_mode(GPIOB, 8, MODE_INPUT);
  register_set(&(SYSCFG->EXTICR[2]), SYSCFG_EXTICR3_EXTI8_PB, 0xFU);
  set_gpio_mode(GPIOB, 5, MODE_INPUT);
  register_set(&(SYSCFG->EXTICR[1]), SYSCFG_EXTICR2_EXTI5_PB, 0xF0U);
  set_gpio_mode(GPIOD, 12, MODE_INPUT);
  register_set(&(SYSCFG->EXTICR[3]), SYSCFG_EXTICR4_EXTI12_PD, 0xFU);
  uint32_t can_exti_line = (1UL << 8) | (1UL << 5) | (1UL << 12);
  register_set_bits(&(EXTI->IMR1), can_exti_line);
  register_set_bits(&(EXTI->FTSR1), can_exti_line);
  EXTI->PR1 = (1U << 1) | (1U << 4) | can_exti_line;
  if (harness_check_ignition()) { NVIC_SystemReset(); }
  register_clear_bits(&(PWR->CPUCR), PWR_CPUCR_PDDS_D1 | PWR_CPUCR_PDDS_D2 | PWR_CPUCR_PDDS_D3);
  register_set(&(PWR->CR1), PWR_CR1_SVOS_0 | PWR_CR1_FLPS, PWR_CR1_SVOS | PWR_CR1_FLPS);
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
  __disable_irq();
  for (uint32_t i = 0U; i < 8U; i++) { NVIC->ICER[i] = 0xFFFFFFFFU; NVIC->ICPR[i] = 0xFFFFFFFFU; }
  NVIC_EnableIRQ(EXTI1_IRQn); NVIC_EnableIRQ(EXTI4_IRQn); NVIC_EnableIRQ(EXTI9_5_IRQn); NVIC_EnableIRQ(EXTI15_10_IRQn);
  __DSB(); __ISB(); __WFI();
  NVIC_SystemReset();
}
