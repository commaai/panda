#pragma once

#define CADC_FILTERING 8U
#define ADC_RAW_TO_mV(x) ((((uint32_t)(x) * current_board->avdd_mV) / 65535U) * adc_cal_factor / 1000UL)

#define VREFINT_CAL_ADDR   ((uint16_t *)0x1FF1E860UL)

struct cadc_state_t {
  uint8_t current_channel;
  uint32_t voltage_raw;
  uint32_t current_raw;
};

extern struct cadc_state_t cadc_state;
extern uint32_t adc_cal_factor;

void adc_calibrate(void);
