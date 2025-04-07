// minimal code to fake a panda for tests
#pragma once
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "safety/board/fake_stm.h"

#define CANFD

#define ENTER_CRITICAL() 0
#define EXIT_CRITICAL() 0

typedef uint32_t GPIO_TypeDef;
typedef enum IRQn_Type irqn_typedef;

static uint8_t global_critical_depth = 0U;

static void __disable_irq(void) {};
static void __enable_irq(void) {};

volatile bool interrupts_enabled = false;
