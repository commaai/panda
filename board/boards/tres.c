#include "board/config.h"
#include "board/utils.h"
#include "board/drivers/gpio.h"
#include "board/drivers/harness.h"
#include "board/drivers/led.h"
#include "board/stm32h7/lladc.h"
#include "board/boards/unused_funcs.h"
#include "board/boards/tres.h"

#ifndef BOOTSTUB
static void tres_enable_can_transceiver(uint8_t transceiver, bool enabled) {
  switch (transceiver) {
    case 1U:
      set_gpio_output(GPIOG, 11, !enabled);
      break;
    case 2U:
      set_gpio_output(GPIOB, 3, !enabled);
      break;
    case 3U:
      set_gpio_output(GPIOD, 7, !enabled);
      break;
    case 4U:
      set_gpio_output(GPIOB, 4, !enabled);
      break;
    default:
      break;
  }
}

void tres_set_can_mode(uint8_t mode) {
  tres_enable_can_transceiver(2U, false);
  tres_enable_can_transceiver(4U, false);
  switch (mode) {
    case CAN_MODE_NORMAL:
    case CAN_MODE_OBD_CAN2:
      if ((bool)(mode == CAN_MODE_NORMAL) != (bool)(harness.status == HARNESS_STATUS_FLIPPED)) {
        // B12,B13: disable normal mode
        set_gpio_pullup(GPIOB, 12, PULL_NONE);
        set_gpio_mode(GPIOB, 12, MODE_ANALOG);

        set_gpio_pullup(GPIOB, 13, PULL_NONE);
        set_gpio_mode(GPIOB, 13, MODE_ANALOG);

        // B5,B6: FDCAN2 mode
        set_gpio_pullup(GPIOB, 5, PULL_NONE);
        set_gpio_alternate(GPIOB, 5, GPIO_AF9_FDCAN2);

        set_gpio_pullup(GPIOB, 6, PULL_NONE);
        set_gpio_alternate(GPIOB, 6, GPIO_AF9_FDCAN2);
        tres_enable_can_transceiver(2U, true);
      } else {
        // B5,B6: disable normal mode
        set_gpio_pullup(GPIOB, 5, PULL_NONE);
        set_gpio_mode(GPIOB, 5, MODE_ANALOG);

        set_gpio_pullup(GPIOB, 6, PULL_NONE);
        set_gpio_mode(GPIOB, 6, MODE_ANALOG);
        // B12,B13: FDCAN2 mode
        set_gpio_pullup(GPIOB, 12, PULL_NONE);
        set_gpio_alternate(GPIOB, 12, GPIO_AF9_FDCAN2);

        set_gpio_pullup(GPIOB, 13, PULL_NONE);
        set_gpio_alternate(GPIOB, 13, GPIO_AF9_FDCAN2);
        tres_enable_can_transceiver(4U, true);
      }
      break;
    default:
      break;
  }
}

static uint32_t tres_read_voltage_mV(void){
  return adc_get_mV(&(const adc_signal_t) ADC_CHANNEL_DEFAULT(ADC1, 2)) * 11U;
}

bool tres_read_som_gpio(void) {
  return (GPIOC->IDR & (1UL << 1)) == 0U;
}

static void tres_init(void) {
  common_init_gpio();

  // G11,B3,D7,B4: transceiver enable
  set_gpio_pullup(GPIOG, 11, PULL_NONE);
  set_gpio_mode(GPIOG, 11, MODE_OUTPUT);

  set_gpio_pullup(GPIOB, 3, PULL_NONE);
  set_gpio_mode(GPIOB, 3, MODE_OUTPUT);

  set_gpio_pullup(GPIOD, 7, PULL_NONE);
  set_gpio_mode(GPIOD, 7, MODE_OUTPUT);

  set_gpio_pullup(GPIOB, 4, PULL_NONE);
  set_gpio_mode(GPIOB, 4, MODE_OUTPUT);

  //B1: 5VOUT_S
  set_gpio_pullup(GPIOB, 1, PULL_NONE);
  set_gpio_mode(GPIOB, 1, MODE_ANALOG);

  // B14: usb load switch
  set_gpio_output_type(GPIOB, 14, OUTPUT_TYPE_OPEN_DRAIN);
  set_gpio_pullup(GPIOB, 14, PULL_UP);
  set_gpio_mode(GPIOB, 14, MODE_OUTPUT);
  set_gpio_output(GPIOB, 14, 1);

  // C1: SOM_GPIO
  set_gpio_mode(GPIOC, 1, MODE_INPUT);
  set_gpio_pullup(GPIOC, 1, PULL_UP);
}

static harness_configuration tres_harness_config = {
  .has_harness = true,
  .GPIO_SBU1 = GPIOC,
  .GPIO_SBU2 = GPIOA,
  .GPIO_relay_SBU1 = GPIOC,
  .GPIO_relay_SBU2 = GPIOC,
  .pin_SBU1 = 4,
  .pin_SBU2 = 1,
  .pin_relay_SBU1 = 10,
  .pin_relay_SBU2 = 11,
  .adc_signal_SBU1 = ADC_CHANNEL_DEFAULT(ADC1, 4),
  .adc_signal_SBU2 = ADC_CHANNEL_DEFAULT(ADC1, 17)
};
#endif

board board_tres = {
  .harness_config = 
#ifndef BOOTSTUB
    &tres_harness_config,
#else
    NULL,
#endif
  .has_spi = true,
  .has_fan = true,
  .avdd_mV = 3300U,
  .fan_enable_cooldown_time = 0U,
  .init = 
#ifndef BOOTSTUB
    tres_init,
#else
    NULL,
#endif
  .init_bootloader = NULL,
  .enable_can_transceiver = 
#ifndef BOOTSTUB
    tres_enable_can_transceiver,
#else
    NULL,
#endif
  .led_GPIO = {GPIOE, GPIOE, GPIOE},
  .led_pin = {4, 3, 2},
  .set_can_mode = 
#ifndef BOOTSTUB
    tres_set_can_mode,
#else
    NULL,
#endif
  .read_voltage_mV = 
#ifndef BOOTSTUB
    tres_read_voltage_mV,
#else
    NULL,
#endif
  .read_current_mA = unused_read_current_mA,
  .set_fan_enabled = unused_set_fan_enabled,
  .set_ir_power = unused_set_ir_power,
  .set_siren = unused_set_siren,
  .read_som_gpio = 
#ifndef BOOTSTUB
    tres_read_som_gpio,
#else
    unused_read_som_gpio,
#endif
  .set_amp_enabled = unused_set_amp_enabled,
  .set_bootkick = unused_set_bootkick,
  .board_comms_handler = unused_comms_control_handler
};
