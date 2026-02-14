#include "power_saving_declarations.h"

// CoU_3 (STM UM1840): Hardware low-power modes must NOT be used during safety function execution.
// Stop mode is only entered from SAFETY_SILENT when no safety function is active.

int power_save_status = POWER_SAVE_STATUS_DISABLED;

void enable_can_transceivers(bool enabled) {
  // Leave main CAN always on for CAN-based ignition detection
  uint8_t main_bus = (harness.status == HARNESS_STATUS_FLIPPED) ? 3U : 1U;
  for(uint8_t i=1U; i<=4U; i++){
    current_board->enable_can_transceiver(i, (i == main_bus) || enabled);
  }
}

void set_power_save_state(int state) {
  bool is_valid_state = (state == POWER_SAVE_STATUS_ENABLED) || (state == POWER_SAVE_STATUS_DISABLED);
  if (is_valid_state && (state != power_save_status)) {
    bool enable = false;
    if (state == POWER_SAVE_STATUS_ENABLED) {
      print("enable power savings\n");

      // Disable CAN interrupts
      if (harness.status == HARNESS_STATUS_FLIPPED) {
        llcan_irq_disable(cans[0]);
      } else {
        llcan_irq_disable(cans[2]);
      }
      llcan_irq_disable(cans[1]);
    } else {
      print("disable power savings\n");

      if (harness.status == HARNESS_STATUS_FLIPPED) {
        llcan_irq_enable(cans[0]);
      } else {
        llcan_irq_enable(cans[2]);
      }
      llcan_irq_enable(cans[1]);

      enable = true;
    }

    enable_can_transceivers(enable);

    // Switch off IR when in power saving
    if(!enable){
      current_board->set_ir_power(0U);
    }

    power_save_status = state;
  }
}

static void enter_stop_mode(void) {
  // set all to analog mode to reduce leakage current
  GPIOA->MODER = 0xFFFFFFFFU;
  GPIOB->MODER = 0xFFFFFFFFU;
  GPIOC->MODER = 0xFFFFFFFFU;
  GPIOD->MODER = 0xFFFFFFFFU;
  GPIOE->MODER = 0xFFFFFFFFU;
  GPIOF->MODER = 0xFFFFFFFFU;
  GPIOG->MODER = 0xFFFFFFFFU;

  // re-enable GPIO states
  current_board->set_bootkick(BOOT_STANDBY);
  led_set(LED_RED, false);
  led_set(LED_GREEN, false);
  led_set(LED_BLUE, false);
  current_board->set_fan_enabled(false);
  current_board->set_amp_enabled(false);
  for (uint8_t i = 1U; i <= 4U; i++) {
    current_board->enable_can_transceiver(i, false);
  }

  // disable SRAM retention in stop mode (content not needed, we reset on wakeup)
  register_clear_bits(&(RCC->AHB2LPENR), RCC_AHB2LPENR_SRAM1LPEN | RCC_AHB2LPENR_SRAM2LPEN);
  register_clear_bits(&(RCC->AHB4LPENR), RCC_AHB4LPENR_SRAM4LPEN);
  register_clear_bits(&(RCC->AHB3LPENR), RCC_AHB3LPENR_AXISRAMLPEN);

  // SBU pins to input for EXTI wakeup
  set_gpio_mode(current_board->harness_config->GPIO_SBU1,
                current_board->harness_config->pin_SBU1, MODE_INPUT);
  set_gpio_mode(current_board->harness_config->GPIO_SBU2,
                current_board->harness_config->pin_SBU2, MODE_INPUT);

  // SBU EXTI: EXTI1 -> PA1 (SBU2), EXTI4 -> PC4 (SBU1)
  register_set(&(SYSCFG->EXTICR[0]), SYSCFG_EXTICR1_EXTI1_PA, 0xF0U);
  register_set(&(SYSCFG->EXTICR[1]), SYSCFG_EXTICR2_EXTI4_PC, 0xFU);
  register_set_bits(&(EXTI->IMR1), (1U << 1) | (1U << 4));
  register_set_bits(&(EXTI->RTSR1), (1U << 1) | (1U << 4));
  register_set_bits(&(EXTI->FTSR1), (1U << 1) | (1U << 4));

  // CAN RX EXTI for CAN ignition
  // Normal:  FDCAN1 RX = PB8  (EXTI8)
  // Flipped: FDCAN3 RX = PD12 (EXTI12)
  uint32_t can_exti_line;
  if (harness.status == HARNESS_STATUS_FLIPPED) {
    set_gpio_mode(GPIOD, 12, MODE_INPUT);
    register_set(&(SYSCFG->EXTICR[3]), SYSCFG_EXTICR4_EXTI12_PD, 0xFU);
    can_exti_line = (1U << 12);
  } else {
    set_gpio_mode(GPIOB, 8, MODE_INPUT);
    register_set(&(SYSCFG->EXTICR[2]), SYSCFG_EXTICR3_EXTI8_PB, 0xFU);
    can_exti_line = (1U << 8);
  }
  register_set_bits(&(EXTI->IMR1), can_exti_line);
  register_set_bits(&(EXTI->FTSR1), can_exti_line);

  // disable all NVIC interrupts and clear pending
  for (uint32_t i = 0U; i < 8U; i++) {
    NVIC->ICER[i] = 0xFFFFFFFFU;
    NVIC->ICPR[i] = 0xFFFFFFFFU;
  }

  // enable only wakeup EXTI lines
  NVIC_EnableIRQ(EXTI1_IRQn);     // SBU2 (PA1)
  NVIC_EnableIRQ(EXTI4_IRQn);     // SBU1 (PC4)
  if (harness.status == HARNESS_STATUS_FLIPPED) {
    NVIC_EnableIRQ(EXTI15_10_IRQn);  // CAN3 RX (PD12)
  } else {
    NVIC_EnableIRQ(EXTI9_5_IRQn);    // CAN1 RX (PB8)
  }

  // check if ignition is already on
  EXTI->PR1 = (1U << 1) | (1U << 4) | can_exti_line;
  if (harness_check_ignition()) {
    NVIC_SystemReset();
  }

  // stop mode (not standby)
  register_clear_bits(&(PWR->CPUCR), PWR_CPUCR_PDDS_D1 | PWR_CPUCR_PDDS_D2 | PWR_CPUCR_PDDS_D3);

  // SVOS5 + flash low-power
  register_set(&(PWR->CR1), PWR_CR1_SVOS_0 | PWR_CR1_FLPS, PWR_CR1_SVOS | PWR_CR1_FLPS);

  // enter stop mode on WFI
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

  __disable_irq();
  __DSB();
  __ISB();
  __WFI();

  NVIC_SystemReset();
}
