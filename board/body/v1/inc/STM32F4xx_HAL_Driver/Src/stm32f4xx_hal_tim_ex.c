#include "stm32f4xx_hal.h"

#ifdef HAL_TIM_MODULE_ENABLED

static void TIM_CCxNChannelCmd(TIM_TypeDef *TIMx, uint32_t Channel, uint32_t ChannelNState);

HAL_StatusTypeDef HAL_TIMEx_OCN_Start(TIM_HandleTypeDef *htim, uint32_t Channel) {
  uint32_t tmpsmcr;

  assert_param(IS_TIM_CCXN_INSTANCE(htim->Instance, Channel));

  if (TIM_CHANNEL_N_STATE_GET(htim, Channel) != HAL_TIM_CHANNEL_STATE_READY) {
    return HAL_ERROR;
  }

  TIM_CHANNEL_N_STATE_SET(htim, Channel, HAL_TIM_CHANNEL_STATE_BUSY);
  TIM_CCxNChannelCmd(htim->Instance, Channel, TIM_CCxN_ENABLE);
  __HAL_TIM_MOE_ENABLE(htim);

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

HAL_StatusTypeDef HAL_TIMEx_PWMN_Start(TIM_HandleTypeDef *htim, uint32_t Channel) {
  uint32_t tmpsmcr;

  assert_param(IS_TIM_CCXN_INSTANCE(htim->Instance, Channel));

  if (TIM_CHANNEL_N_STATE_GET(htim, Channel) != HAL_TIM_CHANNEL_STATE_READY) {
    return HAL_ERROR;
  }

  TIM_CHANNEL_N_STATE_SET(htim, Channel, HAL_TIM_CHANNEL_STATE_BUSY);
  TIM_CCxNChannelCmd(htim->Instance, Channel, TIM_CCxN_ENABLE);
  __HAL_TIM_MOE_ENABLE(htim);

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

HAL_StatusTypeDef HAL_TIMEx_MasterConfigSynchronization(TIM_HandleTypeDef *htim,
                                                        TIM_MasterConfigTypeDef *sMasterConfig) {
  uint32_t tmpcr2;
  uint32_t tmpsmcr;

  assert_param(IS_TIM_MASTER_INSTANCE(htim->Instance));
  assert_param(IS_TIM_TRGO_SOURCE(sMasterConfig->MasterOutputTrigger));
  assert_param(IS_TIM_MSM_STATE(sMasterConfig->MasterSlaveMode));

  __HAL_LOCK(htim);
  htim->State = HAL_TIM_STATE_BUSY;

  tmpcr2 = htim->Instance->CR2;
  tmpsmcr = htim->Instance->SMCR;

  tmpcr2 &= ~TIM_CR2_MMS;
  tmpcr2 |= sMasterConfig->MasterOutputTrigger;
  htim->Instance->CR2 = tmpcr2;

  if (IS_TIM_SLAVE_INSTANCE(htim->Instance)) {
    tmpsmcr &= ~TIM_SMCR_MSM;
    tmpsmcr |= sMasterConfig->MasterSlaveMode;
    htim->Instance->SMCR = tmpsmcr;
  }

  htim->State = HAL_TIM_STATE_READY;
  __HAL_UNLOCK(htim);
  return HAL_OK;
}

HAL_StatusTypeDef HAL_TIMEx_ConfigBreakDeadTime(TIM_HandleTypeDef *htim,
                                                TIM_BreakDeadTimeConfigTypeDef *sBreakDeadTimeConfig) {
  uint32_t tmpbdtr = 0U;

  assert_param(IS_TIM_BREAK_INSTANCE(htim->Instance));
  assert_param(IS_TIM_OSSR_STATE(sBreakDeadTimeConfig->OffStateRunMode));
  assert_param(IS_TIM_OSSI_STATE(sBreakDeadTimeConfig->OffStateIDLEMode));
  assert_param(IS_TIM_LOCK_LEVEL(sBreakDeadTimeConfig->LockLevel));
  assert_param(IS_TIM_DEADTIME(sBreakDeadTimeConfig->DeadTime));
  assert_param(IS_TIM_BREAK_STATE(sBreakDeadTimeConfig->BreakState));
  assert_param(IS_TIM_BREAK_POLARITY(sBreakDeadTimeConfig->BreakPolarity));
  assert_param(IS_TIM_AUTOMATIC_OUTPUT_STATE(sBreakDeadTimeConfig->AutomaticOutput));

  __HAL_LOCK(htim);

  MODIFY_REG(tmpbdtr, TIM_BDTR_DTG, sBreakDeadTimeConfig->DeadTime);
  MODIFY_REG(tmpbdtr, TIM_BDTR_LOCK, sBreakDeadTimeConfig->LockLevel);
  MODIFY_REG(tmpbdtr, TIM_BDTR_OSSI, sBreakDeadTimeConfig->OffStateIDLEMode);
  MODIFY_REG(tmpbdtr, TIM_BDTR_OSSR, sBreakDeadTimeConfig->OffStateRunMode);
  MODIFY_REG(tmpbdtr, TIM_BDTR_BKE, sBreakDeadTimeConfig->BreakState);
  MODIFY_REG(tmpbdtr, TIM_BDTR_BKP, sBreakDeadTimeConfig->BreakPolarity);
  MODIFY_REG(tmpbdtr, TIM_BDTR_AOE, sBreakDeadTimeConfig->AutomaticOutput);

  htim->Instance->BDTR = tmpbdtr;

  __HAL_UNLOCK(htim);
  return HAL_OK;
}

static void TIM_CCxNChannelCmd(TIM_TypeDef *TIMx, uint32_t Channel, uint32_t ChannelNState) {
  uint32_t tmp;

  tmp = TIM_CCER_CC1NE << (Channel & 0x1FU);
  TIMx->CCER &= ~tmp;
  TIMx->CCER |= (uint32_t)(ChannelNState << (Channel & 0x1FU));
}

#endif
