#ifndef LLADC_DECLARATIONS_H
#define LLADC_DECLARATIONS_H

#include "board/drivers/driver_declarations.h"

#define SAMPLETIME_1_5_CYCLES 0U
#define SAMPLETIME_2_5_CYCLES 1U
#define SAMPLETIME_8_5_CYCLES 2U
#define SAMPLETIME_16_5_CYCLES 3U
#define SAMPLETIME_32_5_CYCLES 4U
#define SAMPLETIME_64_5_CYCLES 5U
#define SAMPLETIME_387_5_CYCLES 6U
#define SAMPLETIME_810_CYCLES 7U

#define OVERSAMPLING_1 0U

#define ADC_CHANNEL_DEFAULT(a, c) {.adc = (a), .channel = (c), .sample_time = SAMPLETIME_810_CYCLES, .oversampling = OVERSAMPLING_1}

#endif
