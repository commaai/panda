#include "stm32f4xx_hal.h"

#ifdef HAL_TIM_MODULE_ENABLED

static void TIM_OC1_SetConfig(TIM_TypeDef *TIMx, TIM_OC_InitTypeDef *OC_Config);
static void TIM_OC3_SetConfig(TIM_TypeDef *TIMx, TIM_OC_InitTypeDef *OC_Config);
static void TIM_OC4_SetConfig(TIM_TypeDef *TIMx, TIM_OC_InitTypeDef *OC_Config);
static HAL_StatusTypeDef TIM_SlaveTimer_SetConfig(TIM_HandleTypeDef *htim,
                                                  TIM_SlaveConfigTypeDef *sSlaveConfig);

HAL_StatusTypeDef HAL_TIM_OC_Start(TIM_HandleTypeDef *htim, uint32_t Channel) {
  uint32_t tmpsmcr;

  assert_param(IS_TIM_CCX_INSTANCE(htim->Instance, Channel));

  if (TIM_CHANNEL_STATE_GET(htim, Channel) != HAL_TIM_CHANNEL_STATE_READY) {
    return HAL_ERROR;
  }

  TIM_CHANNEL_STATE_SET(htim, Channel, HAL_TIM_CHANNEL_STATE_BUSY);
  TIM_CCxChannelCmd(htim->Instance, Channel, TIM_CCx_ENABLE);

  if (IS_TIM_BREAK_INSTANCE(htim->Instance) != RESET) {
    __HAL_TIM_MOE_ENABLE(htim);
  }

  if (IS_TIM_SLAVE_INSTANCE(htim->Instance)) {
    tmpsmcr = htim->Instance->SMCR & TIM_SMCR_SMS;
    if (!IS_TIM_SLAVEMODE_TRIGGER_ENABLED(tmpsmcr)) {
      __HAL_TIM_ENABLE(htim);
    }
  } else {
    __HAL_TIM_ENABLE(htim);
  }

  return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_PWM_Init(TIM_HandleTypeDef *htim) {
  if (htim == NULL) {
    return HAL_ERROR;
  }

  assert_param(IS_TIM_INSTANCE(htim->Instance));
  assert_param(IS_TIM_COUNTER_MODE(htim->Init.CounterMode));
  assert_param(IS_TIM_CLOCKDIVISION_DIV(htim->Init.ClockDivision));
  assert_param(IS_TIM_AUTORELOAD_PRELOAD(htim->Init.AutoReloadPreload));

  if (htim->State == HAL_TIM_STATE_RESET) {
    htim->Lock = HAL_UNLOCKED;

#if (USE_HAL_TIM_REGISTER_CALLBACKS == 1)
    if (htim->PWM_MspInitCallback == NULL) {
      htim->PWM_MspInitCallback = HAL_TIM_PWM_MspInit;
    }
    htim->PWM_MspInitCallback(htim);
#else
    HAL_TIM_PWM_MspInit(htim);
#endif
  }

  htim->State = HAL_TIM_STATE_BUSY;
  TIM_Base_SetConfig(htim->Instance, &htim->Init);
  htim->DMABurstState = HAL_DMA_BURST_STATE_READY;
  TIM_CHANNEL_STATE_SET_ALL(htim, HAL_TIM_CHANNEL_STATE_READY);
  TIM_CHANNEL_N_STATE_SET_ALL(htim, HAL_TIM_CHANNEL_STATE_READY);
  htim->State = HAL_TIM_STATE_READY;
  return HAL_OK;
}

__weak void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim) {
  UNUSED(htim);
}

HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *htim, uint32_t Channel) {
  uint32_t tmpsmcr;

  assert_param(IS_TIM_CCX_INSTANCE(htim->Instance, Channel));

  if (TIM_CHANNEL_STATE_GET(htim, Channel) != HAL_TIM_CHANNEL_STATE_READY) {
    return HAL_ERROR;
  }

  TIM_CHANNEL_STATE_SET(htim, Channel, HAL_TIM_CHANNEL_STATE_BUSY);
  TIM_CCxChannelCmd(htim->Instance, Channel, TIM_CCx_ENABLE);

  if (IS_TIM_BREAK_INSTANCE(htim->Instance) != RESET) {
    __HAL_TIM_MOE_ENABLE(htim);
  }

  if (IS_TIM_SLAVE_INSTANCE(htim->Instance)) {
    tmpsmcr = htim->Instance->SMCR & TIM_SMCR_SMS;
    if (!IS_TIM_SLAVEMODE_TRIGGER_ENABLED(tmpsmcr)) {
      __HAL_TIM_ENABLE(htim);
    }
  } else {
    __HAL_TIM_ENABLE(htim);
  }

  return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_PWM_ConfigChannel(TIM_HandleTypeDef *htim,
                                            TIM_OC_InitTypeDef *sConfig,
                                            uint32_t Channel) {
  HAL_StatusTypeDef status = HAL_OK;

  assert_param(IS_TIM_CHANNELS(Channel));
  assert_param(IS_TIM_PWM_MODE(sConfig->OCMode));
  assert_param(IS_TIM_OC_POLARITY(sConfig->OCPolarity));
  assert_param(IS_TIM_FAST_STATE(sConfig->OCFastMode));

  __HAL_LOCK(htim);

  switch (Channel) {
    case TIM_CHANNEL_1:
      assert_param(IS_TIM_CC1_INSTANCE(htim->Instance));
      TIM_OC1_SetConfig(htim->Instance, sConfig);
      htim->Instance->CCMR1 |= TIM_CCMR1_OC1PE;
      htim->Instance->CCMR1 &= ~TIM_CCMR1_OC1FE;
      htim->Instance->CCMR1 |= sConfig->OCFastMode;
      break;
    case TIM_CHANNEL_2:
      assert_param(IS_TIM_CC2_INSTANCE(htim->Instance));
      TIM_OC2_SetConfig(htim->Instance, sConfig);
      htim->Instance->CCMR1 |= TIM_CCMR1_OC2PE;
      htim->Instance->CCMR1 &= ~TIM_CCMR1_OC2FE;
      htim->Instance->CCMR1 |= sConfig->OCFastMode << 8U;
      break;
    case TIM_CHANNEL_3:
      assert_param(IS_TIM_CC3_INSTANCE(htim->Instance));
      TIM_OC3_SetConfig(htim->Instance, sConfig);
      htim->Instance->CCMR2 |= TIM_CCMR2_OC3PE;
      htim->Instance->CCMR2 &= ~TIM_CCMR2_OC3FE;
      htim->Instance->CCMR2 |= sConfig->OCFastMode;
      break;
    case TIM_CHANNEL_4:
      assert_param(IS_TIM_CC4_INSTANCE(htim->Instance));
      TIM_OC4_SetConfig(htim->Instance, sConfig);
      htim->Instance->CCMR2 |= TIM_CCMR2_OC4PE;
      htim->Instance->CCMR2 &= ~TIM_CCMR2_OC4FE;
      htim->Instance->CCMR2 |= sConfig->OCFastMode << 8U;
      break;
    default:
      status = HAL_ERROR;
      break;
  }

  __HAL_UNLOCK(htim);
  return status;
}

HAL_StatusTypeDef HAL_TIM_SlaveConfigSynchro(TIM_HandleTypeDef *htim, TIM_SlaveConfigTypeDef *sSlaveConfig) {
  assert_param(IS_TIM_SLAVE_INSTANCE(htim->Instance));
  assert_param(IS_TIM_SLAVE_MODE(sSlaveConfig->SlaveMode));
  assert_param(IS_TIM_TRIGGER_SELECTION(sSlaveConfig->InputTrigger));

  __HAL_LOCK(htim);
  htim->State = HAL_TIM_STATE_BUSY;

  if (TIM_SlaveTimer_SetConfig(htim, sSlaveConfig) != HAL_OK) {
    htim->State = HAL_TIM_STATE_READY;
    __HAL_UNLOCK(htim);
    return HAL_ERROR;
  }

  __HAL_TIM_DISABLE_IT(htim, TIM_IT_TRIGGER);
  __HAL_TIM_DISABLE_DMA(htim, TIM_DMA_TRIGGER);
  htim->State = HAL_TIM_STATE_READY;
  __HAL_UNLOCK(htim);
  return HAL_OK;
}

void TIM_Base_SetConfig(TIM_TypeDef *TIMx, TIM_Base_InitTypeDef *Structure) {
  uint32_t tmpcr1 = TIMx->CR1;

  if (IS_TIM_COUNTER_MODE_SELECT_INSTANCE(TIMx)) {
    tmpcr1 &= ~(TIM_CR1_DIR | TIM_CR1_CMS);
    tmpcr1 |= Structure->CounterMode;
  }

  if (IS_TIM_CLOCK_DIVISION_INSTANCE(TIMx)) {
    tmpcr1 &= ~TIM_CR1_CKD;
    tmpcr1 |= (uint32_t)Structure->ClockDivision;
  }

  MODIFY_REG(tmpcr1, TIM_CR1_ARPE, Structure->AutoReloadPreload);
  TIMx->CR1 = tmpcr1;
  TIMx->ARR = (uint32_t)Structure->Period;
  TIMx->PSC = Structure->Prescaler;

  if (IS_TIM_REPETITION_COUNTER_INSTANCE(TIMx)) {
    TIMx->RCR = Structure->RepetitionCounter;
  }

  TIMx->EGR = TIM_EGR_UG;
}

static void TIM_OC1_SetConfig(TIM_TypeDef *TIMx, TIM_OC_InitTypeDef *OC_Config) {
  uint32_t tmpccmrx;
  uint32_t tmpccer;
  uint32_t tmpcr2;

  TIMx->CCER &= ~TIM_CCER_CC1E;
  tmpccer = TIMx->CCER;
  tmpcr2 = TIMx->CR2;
  tmpccmrx = TIMx->CCMR1;

  tmpccmrx &= ~TIM_CCMR1_OC1M;
  tmpccmrx &= ~TIM_CCMR1_CC1S;
  tmpccmrx |= OC_Config->OCMode;

  tmpccer &= ~TIM_CCER_CC1P;
  tmpccer |= OC_Config->OCPolarity;

  if (IS_TIM_CCXN_INSTANCE(TIMx, TIM_CHANNEL_1)) {
    assert_param(IS_TIM_OCN_POLARITY(OC_Config->OCNPolarity));
    tmpccer &= ~TIM_CCER_CC1NP;
    tmpccer |= OC_Config->OCNPolarity;
    tmpccer &= ~TIM_CCER_CC1NE;
  }

  if (IS_TIM_BREAK_INSTANCE(TIMx)) {
    assert_param(IS_TIM_OCNIDLE_STATE(OC_Config->OCNIdleState));
    assert_param(IS_TIM_OCIDLE_STATE(OC_Config->OCIdleState));
    tmpcr2 &= ~TIM_CR2_OIS1;
    tmpcr2 &= ~TIM_CR2_OIS1N;
    tmpcr2 |= OC_Config->OCIdleState;
    tmpcr2 |= OC_Config->OCNIdleState;
  }

  TIMx->CR2 = tmpcr2;
  TIMx->CCMR1 = tmpccmrx;
  TIMx->CCR1 = OC_Config->Pulse;
  TIMx->CCER = tmpccer;
}

void TIM_OC2_SetConfig(TIM_TypeDef *TIMx, TIM_OC_InitTypeDef *OC_Config) {
  uint32_t tmpccmrx;
  uint32_t tmpccer;
  uint32_t tmpcr2;

  TIMx->CCER &= ~TIM_CCER_CC2E;
  tmpccer = TIMx->CCER;
  tmpcr2 = TIMx->CR2;
  tmpccmrx = TIMx->CCMR1;

  tmpccmrx &= ~TIM_CCMR1_OC2M;
  tmpccmrx &= ~TIM_CCMR1_CC2S;
  tmpccmrx |= (OC_Config->OCMode << 8U);

  tmpccer &= ~TIM_CCER_CC2P;
  tmpccer |= (OC_Config->OCPolarity << 4U);

  if (IS_TIM_CCXN_INSTANCE(TIMx, TIM_CHANNEL_2)) {
    assert_param(IS_TIM_OCN_POLARITY(OC_Config->OCNPolarity));
    tmpccer &= ~TIM_CCER_CC2NP;
    tmpccer |= (OC_Config->OCNPolarity << 4U);
    tmpccer &= ~TIM_CCER_CC2NE;
  }

  if (IS_TIM_BREAK_INSTANCE(TIMx)) {
    assert_param(IS_TIM_OCNIDLE_STATE(OC_Config->OCNIdleState));
    assert_param(IS_TIM_OCIDLE_STATE(OC_Config->OCIdleState));
    tmpcr2 &= ~TIM_CR2_OIS2;
    tmpcr2 &= ~TIM_CR2_OIS2N;
    tmpcr2 |= (OC_Config->OCIdleState << 2U);
    tmpcr2 |= (OC_Config->OCNIdleState << 2U);
  }

  TIMx->CR2 = tmpcr2;
  TIMx->CCMR1 = tmpccmrx;
  TIMx->CCR2 = OC_Config->Pulse;
  TIMx->CCER = tmpccer;
}

static void TIM_OC3_SetConfig(TIM_TypeDef *TIMx, TIM_OC_InitTypeDef *OC_Config) {
  uint32_t tmpccmrx;
  uint32_t tmpccer;
  uint32_t tmpcr2;

  TIMx->CCER &= ~TIM_CCER_CC3E;
  tmpccer = TIMx->CCER;
  tmpcr2 = TIMx->CR2;
  tmpccmrx = TIMx->CCMR2;

  tmpccmrx &= ~TIM_CCMR2_OC3M;
  tmpccmrx &= ~TIM_CCMR2_CC3S;
  tmpccmrx |= OC_Config->OCMode;

  tmpccer &= ~TIM_CCER_CC3P;
  tmpccer |= (OC_Config->OCPolarity << 8U);

  if (IS_TIM_CCXN_INSTANCE(TIMx, TIM_CHANNEL_3)) {
    assert_param(IS_TIM_OCN_POLARITY(OC_Config->OCNPolarity));
    tmpccer &= ~TIM_CCER_CC3NP;
    tmpccer |= (OC_Config->OCNPolarity << 8U);
    tmpccer &= ~TIM_CCER_CC3NE;
  }

  if (IS_TIM_BREAK_INSTANCE(TIMx)) {
    assert_param(IS_TIM_OCNIDLE_STATE(OC_Config->OCNIdleState));
    assert_param(IS_TIM_OCIDLE_STATE(OC_Config->OCIdleState));
    tmpcr2 &= ~TIM_CR2_OIS3;
    tmpcr2 &= ~TIM_CR2_OIS3N;
    tmpcr2 |= (OC_Config->OCIdleState << 4U);
    tmpcr2 |= (OC_Config->OCNIdleState << 4U);
  }

  TIMx->CR2 = tmpcr2;
  TIMx->CCMR2 = tmpccmrx;
  TIMx->CCR3 = OC_Config->Pulse;
  TIMx->CCER = tmpccer;
}

static void TIM_OC4_SetConfig(TIM_TypeDef *TIMx, TIM_OC_InitTypeDef *OC_Config) {
  uint32_t tmpccmrx;
  uint32_t tmpccer;
  uint32_t tmpcr2;

  TIMx->CCER &= ~TIM_CCER_CC4E;
  tmpccer = TIMx->CCER;
  tmpcr2 = TIMx->CR2;
  tmpccmrx = TIMx->CCMR2;

  tmpccmrx &= ~TIM_CCMR2_OC4M;
  tmpccmrx &= ~TIM_CCMR2_CC4S;
  tmpccmrx |= (OC_Config->OCMode << 8U);

  tmpccer &= ~TIM_CCER_CC4P;
  tmpccer |= (OC_Config->OCPolarity << 12U);

  if (IS_TIM_BREAK_INSTANCE(TIMx)) {
    assert_param(IS_TIM_OCIDLE_STATE(OC_Config->OCIdleState));
    tmpcr2 &= ~TIM_CR2_OIS4;
    tmpcr2 |= (OC_Config->OCIdleState << 6U);
  }

  TIMx->CR2 = tmpcr2;
  TIMx->CCMR2 = tmpccmrx;
  TIMx->CCR4 = OC_Config->Pulse;
  TIMx->CCER = tmpccer;
}

static HAL_StatusTypeDef TIM_SlaveTimer_SetConfig(TIM_HandleTypeDef *htim,
                                                  TIM_SlaveConfigTypeDef *sSlaveConfig) {
  uint32_t tmpsmcr;

  tmpsmcr = htim->Instance->SMCR;
  tmpsmcr &= ~TIM_SMCR_TS;
  tmpsmcr |= sSlaveConfig->InputTrigger;
  tmpsmcr &= ~TIM_SMCR_SMS;
  tmpsmcr |= sSlaveConfig->SlaveMode;
  htim->Instance->SMCR = tmpsmcr;

  switch (sSlaveConfig->InputTrigger) {
    case TIM_TS_ITR0:
    case TIM_TS_ITR1:
    case TIM_TS_ITR2:
    case TIM_TS_ITR3:
      return HAL_OK;
    default:
      return HAL_ERROR;
  }
}

void TIM_CCxChannelCmd(TIM_TypeDef *TIMx, uint32_t Channel, uint32_t ChannelState) {
  uint32_t tmp;

  assert_param(IS_TIM_CC1_INSTANCE(TIMx));
  assert_param(IS_TIM_CHANNELS(Channel));

  tmp = TIM_CCER_CC1E << (Channel & 0x1FU);
  TIMx->CCER &= ~tmp;
  TIMx->CCER |= (uint32_t)(ChannelState << (Channel & 0x1FU));
}

#endif
