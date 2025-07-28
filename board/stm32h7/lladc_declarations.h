#pragma once

#define CADC_FILTERING 8U
#define ADC_RAW_TO_mV(x) (((x) * current_board->avdd_mV * adc_cal_factor) / (65535U * 1000U))

struct cadc_state_t {
  uint8_t current_channel;
  uint32_t voltage_raw;
  uint32_t current_raw;
};

extern struct cadc_state_t cadc_state;
extern uint16_t adc_cal_factor;

void adc_calibrate(void);
