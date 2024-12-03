#pragma once

#define HARNESS_STATUS_NC 0U
#define HARNESS_STATUS_NORMAL 1U
#define HARNESS_STATUS_FLIPPED 2U

struct harness_t {
  uint8_t status;
  uint16_t sbu1_voltage_mV;
  uint16_t sbu2_voltage_mV;
  bool relay_driven;
  bool sbu_adc_lock;
};
extern struct harness_t harness;

struct harness_configuration {
  const bool has_harness;

  // harness control
  GPIO_TypeDef * const GPIO_SBU1;
  GPIO_TypeDef * const GPIO_SBU2;
  GPIO_TypeDef * const GPIO_relay_SBU1;
  GPIO_TypeDef * const GPIO_relay_SBU2;
  const uint8_t pin_SBU1;
  const uint8_t pin_SBU2;
  const uint8_t pin_relay_SBU1;
  const uint8_t pin_relay_SBU2;
  const uint8_t adc_channel_SBU1;
  const uint8_t adc_channel_SBU2;

  // CAN control
  GPIO_TypeDef * const GPIO_CAN2_RX_NORMAL;
  GPIO_TypeDef * const GPIO_CAN2_TX_NORMAL;
  GPIO_TypeDef * const GPIO_CAN2_RX_FLIPPED;
  GPIO_TypeDef * const GPIO_CAN2_TX_FLIPPED;
  const uint8_t pin_CAN2_RX_NORMAL;
  const uint8_t pin_CAN2_TX_NORMAL;
  const uint8_t pin_CAN2_RX_FLIPPED;
  const uint8_t pin_CAN2_TX_FLIPPED;
};

// The ignition relay is only used for testing purposes
void set_intercept_relay(bool intercept, bool ignition_relay);
bool harness_check_ignition(void);
void harness_tick(void);
void harness_init(void);
