#include "stm32f4xx_hal.h"

#ifdef HAL_GPIO_MODULE_ENABLED

#define GPIO_NUMBER 16U

void HAL_GPIO_Init(GPIO_TypeDef *GPIOx, GPIO_InitTypeDef *GPIO_Init) {
  uint32_t position;
  uint32_t ioposition = 0x00U;
  uint32_t iocurrent = 0x00U;
  uint32_t temp = 0x00U;

  assert_param(IS_GPIO_ALL_INSTANCE(GPIOx));
  assert_param(IS_GPIO_PIN(GPIO_Init->Pin));
  assert_param(IS_GPIO_MODE(GPIO_Init->Mode));

  for (position = 0U; position < GPIO_NUMBER; position++) {
    ioposition = 0x01U << position;
    iocurrent = (uint32_t)GPIO_Init->Pin & ioposition;

    if (iocurrent == ioposition) {
      if (((GPIO_Init->Mode & GPIO_MODE) == MODE_OUTPUT) ||
          ((GPIO_Init->Mode & GPIO_MODE) == MODE_AF)) {
        assert_param(IS_GPIO_SPEED(GPIO_Init->Speed));

        temp = GPIOx->OSPEEDR;
        temp &= ~(GPIO_OSPEEDER_OSPEEDR0 << (position * 2U));
        temp |= (GPIO_Init->Speed << (position * 2U));
        GPIOx->OSPEEDR = temp;

        temp = GPIOx->OTYPER;
        temp &= ~(GPIO_OTYPER_OT_0 << position);
        temp |= (((GPIO_Init->Mode & OUTPUT_TYPE) >> OUTPUT_TYPE_Pos) << position);
        GPIOx->OTYPER = temp;
      }

      if ((GPIO_Init->Mode & GPIO_MODE) != MODE_ANALOG) {
        assert_param(IS_GPIO_PULL(GPIO_Init->Pull));

        temp = GPIOx->PUPDR;
        temp &= ~(GPIO_PUPDR_PUPDR0 << (position * 2U));
        temp |= (GPIO_Init->Pull << (position * 2U));
        GPIOx->PUPDR = temp;
      }

      if ((GPIO_Init->Mode & GPIO_MODE) == MODE_AF) {
        assert_param(IS_GPIO_AF(GPIO_Init->Alternate));

        temp = GPIOx->AFR[position >> 3U];
        temp &= ~(0xFU << (((uint32_t)position & 0x07U) * 4U));
        temp |= ((uint32_t)GPIO_Init->Alternate << (((uint32_t)position & 0x07U) * 4U));
        GPIOx->AFR[position >> 3U] = temp;
      }

      temp = GPIOx->MODER;
      temp &= ~(GPIO_MODER_MODER0 << (position * 2U));
      temp |= ((GPIO_Init->Mode & GPIO_MODE) << (position * 2U));
      GPIOx->MODER = temp;

      if ((GPIO_Init->Mode & EXTI_MODE) != 0x00U) {
        __HAL_RCC_SYSCFG_CLK_ENABLE();

        temp = SYSCFG->EXTICR[position >> 2U];
        temp &= ~(0x0FU << (4U * (position & 0x03U)));
        temp |= ((uint32_t)GPIO_GET_INDEX(GPIOx) << (4U * (position & 0x03U)));
        SYSCFG->EXTICR[position >> 2U] = temp;

        temp = EXTI->IMR;
        temp &= ~((uint32_t)iocurrent);
        if ((GPIO_Init->Mode & EXTI_IT) != 0x00U) {
          temp |= iocurrent;
        }
        EXTI->IMR = temp;

        temp = EXTI->EMR;
        temp &= ~((uint32_t)iocurrent);
        if ((GPIO_Init->Mode & EXTI_EVT) != 0x00U) {
          temp |= iocurrent;
        }
        EXTI->EMR = temp;

        temp = EXTI->RTSR;
        temp &= ~((uint32_t)iocurrent);
        if ((GPIO_Init->Mode & TRIGGER_RISING) != 0x00U) {
          temp |= iocurrent;
        }
        EXTI->RTSR = temp;

        temp = EXTI->FTSR;
        temp &= ~((uint32_t)iocurrent);
        if ((GPIO_Init->Mode & TRIGGER_FALLING) != 0x00U) {
          temp |= iocurrent;
        }
        EXTI->FTSR = temp;
      }
    }
  }
}

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
  GPIO_PinState bitstatus;

  assert_param(IS_GPIO_PIN(GPIO_Pin));

  if ((GPIOx->IDR & GPIO_Pin) != (uint32_t)GPIO_PIN_RESET) {
    bitstatus = GPIO_PIN_SET;
  } else {
    bitstatus = GPIO_PIN_RESET;
  }

  return bitstatus;
}

void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState) {
  assert_param(IS_GPIO_PIN(GPIO_Pin));
  assert_param(IS_GPIO_PIN_ACTION(PinState));

  if (PinState != GPIO_PIN_RESET) {
    GPIOx->BSRR = GPIO_Pin;
  } else {
    GPIOx->BSRR = (uint32_t)GPIO_Pin << 16U;
  }
}

void HAL_GPIO_TogglePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
  uint32_t odr;

  assert_param(IS_GPIO_PIN(GPIO_Pin));

  odr = GPIOx->ODR;
  GPIOx->BSRR = ((odr & GPIO_Pin) << GPIO_NUMBER) | (~odr & GPIO_Pin);
}

#endif
