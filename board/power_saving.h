#include "power_saving_declarations.h"

// CoU_3 (STM UM1840): Hardware low-power modes must NOT be used during safety function execution.
// Stop mode is only entered from SAFETY_SILENT after heartbeat timeout — no safety function is active.

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
  // Ensure SOM stays off
  current_board->set_bootkick(BOOT_STANDBY);

  // Turn off power-drawing outputs
  led_set(LED_RED, false);
  led_set(LED_GREEN, false);
  led_set(LED_BLUE, false);
  current_board->set_fan_enabled(false);
  current_board->set_amp_enabled(false);

  // set all transceivers to standby
  for (uint8_t i = 1U; i <= 4U; i++) {
    current_board->enable_can_transceiver(i, false);
  }

  // Set SBU pins to input mode for EXTI wakeup (wire ignition)
  set_gpio_mode(current_board->harness_config->GPIO_SBU1,
                current_board->harness_config->pin_SBU1, MODE_INPUT);
  set_gpio_mode(current_board->harness_config->GPIO_SBU2,
                current_board->harness_config->pin_SBU2, MODE_INPUT);

  // Configure SYSCFG EXTI mux: EXTI1 -> Port A (PA1), EXTI4 -> Port C (PC4)
  register_set(&(SYSCFG->EXTICR[0]), SYSCFG_EXTICR1_EXTI1_PA, SYSCFG_EXTICR1_EXTI1);
  register_set(&(SYSCFG->EXTICR[1]), SYSCFG_EXTICR2_EXTI4_PC, SYSCFG_EXTICR2_EXTI4);

  // Both edges on SBU pins
  register_set_bits(&(EXTI->RTSR1), (1U << 1) | (1U << 4));
  register_set_bits(&(EXTI->FTSR1), (1U << 1) | (1U << 4));
  register_set_bits(&(EXTI->IMR1), (1U << 1) | (1U << 4));

  // CAN RX pin EXTI for CAN-based ignition wakeup
  // Normal: main bus = FDCAN1, RX = PB8 (EXTI8)
  // Flipped: main bus = FDCAN3, RX = PD12 (EXTI12)
  uint32_t can_exti_line;
  if (harness.status == HARNESS_STATUS_FLIPPED) {
    set_gpio_mode(GPIOD, 12, MODE_INPUT);
    register_set(&(SYSCFG->EXTICR[3]), SYSCFG_EXTICR4_EXTI12_PD, SYSCFG_EXTICR4_EXTI12);
    can_exti_line = (1U << 12);
  } else {
    set_gpio_mode(GPIOB, 8, MODE_INPUT);
    register_set(&(SYSCFG->EXTICR[2]), SYSCFG_EXTICR3_EXTI8_PB, SYSCFG_EXTICR3_EXTI8);
    can_exti_line = (1U << 8);
  }
  // Falling edge only — CAN SOF is a dominant (low) bit
  register_set_bits(&(EXTI->FTSR1), can_exti_line);
  register_set_bits(&(EXTI->IMR1), can_exti_line);

  // Disable all NVIC interrupts and clear pending
  for (uint32_t i = 0U; i < 8U; i++) {
    NVIC->ICER[i] = 0xFFFFFFFFU;
    NVIC->ICPR[i] = 0xFFFFFFFFU;
  }

  // Enable only wakeup EXTI lines in NVIC
  NVIC_EnableIRQ(EXTI1_IRQn);     // SBU2 (PA1)
  NVIC_EnableIRQ(EXTI4_IRQn);     // SBU1 (PC4)
  if (harness.status == HARNESS_STATUS_FLIPPED) {
    NVIC_EnableIRQ(EXTI15_10_IRQn);  // CAN3 RX (PD12)
  } else {
    NVIC_EnableIRQ(EXTI9_5_IRQn);    // CAN1 RX (PB8)
  }

  // Clear pending EXTI, then check if ignition is already on
  EXTI->PR1 = (1U << 1) | (1U << 4) | can_exti_line;
  if (harness_check_ignition()) {
    NVIC_SystemReset();
  }

  // Configure PWR for Stop mode: clear PDDS bits (not Standby)
  register_clear_bits(&(PWR->CPUCR), PWR_CPUCR_PDDS_D1 | PWR_CPUCR_PDDS_D2 | PWR_CPUCR_PDDS_D3);

  // SVOS5 (lowest voltage in Stop) + Flash low-power mode
  register_set(&(PWR->CR1), PWR_CR1_SVOS_0 | PWR_CR1_FLPS, PWR_CR1_SVOS | PWR_CR1_FLPS);

  // Set SLEEPDEEP for Stop mode entry
  SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;

  // Mask interrupts at CPU level — WFI still wakes on pending NVIC interrupt
  // with PRIMASK=1 (ARMv7-M B1.5.2), but ISR won't dispatch
  __disable_irq();

  __DSB();
  __ISB();
  __WFI();

  // Woke up — reboot cleanly
  NVIC_SystemReset();
}
