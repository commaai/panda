#pragma once

// Minimal test stubs to allow compilation without STM32 dependencies
#include <stdint.h>
#include <stdbool.h>

// Prevent inclusion of problematic headers in test builds
#define BOARD_CONFIG_H
#define FAKE_STM_H  
#define LIBC_H

// Basic type definitions
#ifndef GPIO_TypeDef
typedef struct { uint32_t dummy; } GPIO_TypeDef;
typedef struct { uint32_t dummy; } TIM_TypeDef; 
typedef struct { uint32_t dummy; } USART_TypeDef;
typedef struct { uint32_t dummy; } CAN_TypeDef;
typedef struct { uint32_t dummy; } USB_OTG_GlobalTypeDef;
#endif

// Memory layout stubs
#define GPIOA ((GPIO_TypeDef *) 0x40020000)
#define GPIOB ((GPIO_TypeDef *) 0x40020400) 
#define GPIOC ((GPIO_TypeDef *) 0x40020800)
#define TIM1  ((TIM_TypeDef *) 0x40010000)
#define TIM2  ((TIM_TypeDef *) 0x40000000)
#define USART1 ((USART_TypeDef *) 0x40011000)
#define USART2 ((USART_TypeDef *) 0x40004400)
#define CAN1  ((CAN_TypeDef *) 0x40006400)
#define CAN2  ((CAN_TypeDef *) 0x40006800)

// Constants  
#define HW_TYPE_UNKNOWN 0U
#define HW_TYPE_PANDA 1U
#define HW_TYPE_PANDA_H7 2U

#define CAN_MODE_NORMAL 0U
#define CAN_MODE_OBD_CAN2 1U

#define SAFETY_SILENT 0xFFFF
#define SAFETY_NOOUTPUT 0x17
#define SAFETY_ELM327 0xE1A
#define SAFETY_ALLOUTPUT 0x1337

#define HARNESS_STATUS_NC 0
#define HARNESS_STATUS_NORMAL 1
#define HARNESS_STATUS_FLIPPED 2

#define LED_RED 0
#define LED_GREEN 1
#define LED_BLUE 2
#define MAX_LED_FADE 10000

// Function stubs - empty implementations for tests
static inline void delay(uint32_t a) { (void)a; }
static inline void print(const char *a) { (void)a; }
static inline void puth(uint32_t i) { (void)i; }
static inline void assert_fatal(bool condition, const char *msg) { 
    if (!condition) { 
        // In tests, just ignore asserts
        (void)msg;
    }
}