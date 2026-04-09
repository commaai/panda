// Define to prevent recursive inclusion
#ifndef CONFIG_H
#define CONFIG_H

#include "stm32f4xx_hal.h"

#define CORE_FREQ               96000000U // MCU frequency in hertz
#define I2C_CLOCKSPEED          100     // I2C clock in kHz
#define PWM_FREQ                16000   // PWM frequency in Hz / is also used for buzzer
#define DEAD_TIME               48      // PWM deadtime
#define DELAY_IN_MAIN_LOOP      5       // in ms. default 5. it is independent of all the timing critical stuff. do not touch if you do not know what you are doing.
#define A2BIT_CONV              50      // A to bit for current conversion on ADC. Example: 1 A = 50, 2 A = 100, etc

#define IGNITION_OFF_DELAY      5       // Stop sending CAN messages after 5 seconds

#define ADC_CONV_CLOCK_CYCLES   (ADC_SAMPLETIME_15CYCLES)
#define ADC_CLOCK_DIV           (4)
#define ADC_TOTAL_CONV_TIME     (ADC_CLOCK_DIV * ADC_CONV_CLOCK_CYCLES) // = ((SystemCoreClock / ADC_CLOCK_HZ) * ADC_CONV_CLOCK_CYCLES), where ADC_CLOCK_HZ = SystemCoreClock/ADC_CLOCK_DIV

#define BAT_FILT_COEF           655       // battery voltage filter coefficient in fixed-point. coef_fixedPoint = coef_floatingPoint * 2^16. In this case 655 = 0.01 * 2^16

#define TEMP_FILT_COEF          655       // temperature filter coefficient in fixed-point. coef_fixedPoint = coef_floatingPoint * 2^16. In this case 655 = 0.01 * 2^16
#define TEMP_CAL_LOW_ADC        945      // temperature 1: ADC value
#define TEMP_CAL_LOW_DEG_C      250       // temperature 1: measured temperature [°C * 10]. Here 35.8 °C
#define TEMP_CAL_HIGH_ADC       949      // temperature 2: ADC value
#define TEMP_CAL_HIGH_DEG_C     251       // temperature 2: measured temperature [°C * 10]. Here 48.9 °C
#define TEMP_WARNING_ENABLE     0         // to beep or not to beep, 1 or 0, DO NOT ACTIVITE WITHOUT CALIBRATION!
#define TEMP_WARNING            600       // annoying fast beeps [°C * 10].  Here 60.0 °C
#define TEMP_POWEROFF_ENABLE    0         // to poweroff or not to poweroff, 1 or 0, DO NOT ACTIVITE WITHOUT CALIBRATION!
#define TEMP_POWEROFF           650       // overheat poweroff. (while not driving) [°C * 10]. Here 65.0 °C

#endif
