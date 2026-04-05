#include "board/config.h"
#include "board/utils.h"
#include "board/stm32h7/lladc.h"

// Prototypes for print/puth if not included via config.h
void print(const char *a);
void puth(unsigned int i);

static uint32_t adc_avdd_mV = 0U;

void adc_init(ADC_TypeDef *adc) {
  adc->CR &= ~(ADC_CR_ADEN); // Disable ADC
  adc->CR &= ~(ADC_CR_DEEPPWD); // Reset deep-power-down mode
  adc->CR |= ADC_CR_ADVREGEN; // Enable ADC regulator
  while(!(adc->ISR & ADC_ISR_LDORDY) && (adc != ADC3));

  adc->CR |= ADC_CR_ADCAL; // Calibrate ADC
  while((adc->CR & ADC_CR_ADCAL) != 0U);

  adc->CR |= ADC_CR_ADEN; // Enable ADC
  while(!(adc->ISR & ADC_ISR_ADRDY));
}

static void adc_calibrate_vdda(void) {
  // Use VREFINT to calibrate VDDA
  // VREFINT is 1.21V nominal (1.18V to 1.24V range)
  // Internal VREFINT channel is on ADC3_INP17
  register_set_bits(&ADC3_COMMON->CCR, ADC_CCR_VREFEN);
  adc_signal_t vrefint_signal = {.adc = ADC3, .channel = 17U};
  uint32_t vrefint_raw = 0U;
  for (uint8_t i = 0U; i < 16U; i++) {
    vrefint_raw += adc_get_raw(&vrefint_signal);
  }
  vrefint_raw /= 16U;

  // VDDA = 1.21V * 4095 / vrefint_raw
  // (we use 1210mV to get VDDA in mV)
  adc_avdd_mV = (1210U * 4095U) / vrefint_raw;

  #ifdef DEBUG
  print("  AVDD: 0x"); puth(adc_avdd_mV); print(" mV\n");
  #endif
}

uint16_t adc_get_raw(const adc_signal_t *signal) {
  signal->adc->SQR1 = (signal->channel << ADC_SQR1_SQ1_Pos);
  signal->adc->CR |= ADC_CR_ADSTART;
  while(!(signal->adc->ISR & ADC_ISR_EOC));
  uint16_t ret = (uint16_t)signal->adc->DR;
  signal->adc->ISR |= ADC_ISR_EOC;
  return ret;
}

uint16_t adc_get_mV(const adc_signal_t *signal) {
  uint32_t ret = 0U;
  if (adc_avdd_mV == 0U) {
    adc_calibrate_vdda();
  }

  if ((signal->adc == ADC1) || (signal->adc == ADC2)) {
    ret = (adc_get_raw(signal) * adc_avdd_mV) / 65535U;
  } else if (signal->adc == ADC3) {
    ret = (adc_get_raw(signal) * adc_avdd_mV) / 4095U;
  }
  return (uint16_t)ret;
}
