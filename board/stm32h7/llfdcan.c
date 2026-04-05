#include "board/config.h"
#include "board/stm32h7/llfdcan.h"
#include "board/stm32h7/llfdcan_declarations.h"

const uint32_t speeds[SPEEDS_ARRAY_SIZE] = {100U, 200U, 500U, 1000U, 1250U, 2500U, 5000U, 10000U};
const uint32_t data_speeds[DATA_SPEEDS_ARRAY_SIZE] = {100U, 200U, 500U, 1000U, 1250U, 2500U, 5000U, 10000U, 20000U, 50000U};

bool fdcan_request_init(FDCAN_GlobalTypeDef *FDCANx) {
  bool ret = true;
  FDCANx->CCCR &= ~(FDCAN_CCCR_CSR);
  while ((FDCANx->CCCR & FDCAN_CCCR_CSA) == FDCAN_CCCR_CSA);
  uint32_t timeout_counter = 0U;
  FDCANx->CCCR |= FDCAN_CCCR_INIT;
  while ((FDCANx->CCCR & FDCAN_CCCR_INIT) == 0U) {
    delay(10000);
    timeout_counter++;
    if (timeout_counter >= CAN_INIT_TIMEOUT_MS){ ret = false; break; }
  }
  return ret;
}

bool fdcan_exit_init(FDCAN_GlobalTypeDef *FDCANx) {
  bool ret = true;
  FDCANx->CCCR &= ~(FDCAN_CCCR_INIT);
  uint32_t timeout_counter = 0U;
  while ((FDCANx->CCCR & FDCAN_CCCR_INIT) != 0U) {
    delay(10000);
    timeout_counter++;
    if (timeout_counter >= CAN_INIT_TIMEOUT_MS) { ret = false; break; }
  }
  return ret;
}

bool llcan_set_speed(FDCAN_GlobalTypeDef *FDCANx, uint32_t speed, uint32_t data_speed, bool non_iso, bool loopback, bool silent) {
  UNUSED(speed);
  bool ret = fdcan_request_init(FDCANx);
  if (ret) {
    FDCANx->CCCR |= FDCAN_CCCR_CCE;
    FDCANx->CCCR &= ~(FDCAN_CCCR_TEST);
    FDCANx->TEST &= ~(FDCAN_TEST_LBCK);
    FDCANx->CCCR &= ~(FDCAN_CCCR_MON);
    FDCANx->CCCR &= ~(FDCAN_CCCR_ASM);
    FDCANx->CCCR &= ~(FDCAN_CCCR_NISO);
    uint8_t prescaler = BITRATE_PRESCALER;
    if (speed < 2500U) { prescaler = BITRATE_PRESCALER * 16U; }
    uint32_t tq = CAN_QUANTA(speed, prescaler);
    uint32_t sp = CAN_SP_NOMINAL;
    uint32_t seg1 = CAN_SEG1(tq, sp);
    uint32_t seg2 = CAN_SEG2(tq, sp);
    uint8_t sjw = MIN(127U, seg2);
    FDCANx->NBTP = (((sjw & 0x7FUL)-1U)<<FDCAN_NBTP_NSJW_Pos) | (((seg1 & 0xFFU)-1U)<<FDCAN_NBTP_NTSEG1_Pos) | (((seg2 & 0x7FU)-1U)<<FDCAN_NBTP_NTSEG2_Pos) | (((prescaler & 0x1FFUL)-1U)<<FDCAN_NBTP_NBRP_Pos);
    if (data_speed == 50000U) { sp = CAN_SP_DATA_5M; } else { sp = CAN_SP_DATA_2M; }
    tq = CAN_QUANTA(data_speed, prescaler);
    seg1 = CAN_SEG1(tq, sp);
    seg2 = CAN_SEG2(tq, sp);
    sjw = MIN(15U, seg2);
    FDCANx->DBTP = (((sjw & 0xFUL)-1U)<<FDCAN_DBTP_DSJW_Pos) | (((seg1 & 0x1FU)-1U)<<FDCAN_DBTP_DTSEG1_Pos) | (((seg2 & 0xFU)-1U)<<FDCAN_DBTP_DTSEG2_Pos) | (((prescaler & 0x1FUL)-1U)<<FDCAN_DBTP_DBRP_Pos);
    if (non_iso) { FDCANx->CCCR |= FDCAN_CCCR_NISO; }
    if (loopback) { FDCANx->CCCR |= FDCAN_CCCR_TEST; FDCANx->TEST |= FDCAN_TEST_LBCK; FDCANx->CCCR |= FDCAN_CCCR_MON; }
    if (silent) { FDCANx->CCCR |= FDCAN_CCCR_MON; }
    ret = fdcan_exit_init(FDCANx);
  }
  return ret;
}

void llcan_irq_disable(const FDCAN_GlobalTypeDef *FDCANx) {
  if (FDCANx == FDCAN1) { NVIC_DisableIRQ(FDCAN1_IT0_IRQn); NVIC_DisableIRQ(FDCAN1_IT1_IRQn); }
  else if (FDCANx == FDCAN2) { NVIC_DisableIRQ(FDCAN2_IT0_IRQn); NVIC_DisableIRQ(FDCAN2_IT1_IRQn); }
  else if (FDCANx == FDCAN3) { NVIC_DisableIRQ(FDCAN3_IT0_IRQn); NVIC_DisableIRQ(FDCAN3_IT1_IRQn); }
}

void llcan_irq_enable(const FDCAN_GlobalTypeDef *FDCANx) {
  if (FDCANx == FDCAN1) { NVIC_EnableIRQ(FDCAN1_IT0_IRQn); NVIC_EnableIRQ(FDCAN1_IT1_IRQn); }
  else if (FDCANx == FDCAN2) { NVIC_EnableIRQ(FDCAN2_IT0_IRQn); NVIC_EnableIRQ(FDCAN2_IT1_IRQn); }
  else if (FDCANx == FDCAN3) { NVIC_EnableIRQ(FDCAN3_IT0_IRQn); NVIC_EnableIRQ(FDCAN3_IT1_IRQn); }
}

bool llcan_init(FDCAN_GlobalTypeDef *FDCANx) {
  uint32_t can_number = CAN_NUM_FROM_CANIF(FDCANx);
  bool ret = fdcan_request_init(FDCANx);
  if (ret) {
    FDCANx->CCCR |= FDCAN_CCCR_CCE;
    FDCANx->CCCR &= ~(FDCAN_CCCR_DAR);
    FDCANx->CCCR |= FDCAN_CCCR_TXP;
    FDCANx->CCCR |= FDCAN_CCCR_PXHD;
    FDCANx->CCCR |= (FDCAN_CCCR_FDOE | FDCAN_CCCR_BRSE);
    FDCANx->TXBC &= ~(FDCAN_TXBC_TFQM);
    FDCANx->TXESC |= 0x7U << FDCAN_TXESC_TBDS_Pos;
    FDCANx->RXESC |= 0x7U << FDCAN_RXESC_F0DS_Pos;
    FDCANx->XIDFC &= ~(FDCAN_XIDFC_LSE);
    FDCANx->SIDFC &= ~(FDCAN_SIDFC_LSS);
    FDCANx->GFC &= ~(FDCAN_GFC_RRFE);
    FDCANx->GFC &= ~(FDCAN_GFC_RRFS);
    FDCANx->GFC &= ~(FDCAN_GFC_ANFE);
    FDCANx->GFC &= ~(FDCAN_GFC_ANFS);
    uint32_t RxFIFO0SA = FDCAN_START_ADDRESS + (can_number * FDCAN_OFFSET);
    uint32_t TxFIFOSA = RxFIFO0SA + (FDCAN_RX_FIFO_0_EL_CNT * FDCAN_RX_FIFO_0_EL_SIZE);
    FDCANx->RXF0C |= (FDCAN_RX_FIFO_0_OFFSET + (can_number * FDCAN_OFFSET_W)) << FDCAN_RXF0C_F0SA_Pos;
    FDCANx->RXF0C |= FDCAN_RX_FIFO_0_EL_CNT << FDCAN_RXF0C_F0S_Pos;
    FDCANx->RXF0C |= FDCAN_RXF0C_F0OM;
    FDCANx->TXBC |= (FDCAN_TX_FIFO_OFFSET + (can_number * FDCAN_OFFSET_W)) << FDCAN_TXBC_TBSA_Pos;
    FDCANx->TXBC |= FDCAN_TX_FIFO_EL_CNT << FDCAN_TXBC_TFQS_Pos;
    uint32_t EndAddress = TxFIFOSA + (FDCAN_TX_FIFO_EL_CNT * FDCAN_TX_FIFO_EL_SIZE);
    for (uint32_t RAMcounter = RxFIFO0SA; RAMcounter < EndAddress; RAMcounter += 4U) { *(uint32_t *)(RAMcounter) = 0x00000000; }
    FDCANx->ILE = (FDCAN_ILE_EINT0 | FDCAN_ILE_EINT1);
    FDCANx->IE &= 0x0U;
    FDCANx->IE |= FDCAN_IE_RF0NE | FDCAN_IE_PEDE | FDCAN_IE_PEAE | FDCAN_IE_BOE | FDCAN_IE_EPE | FDCAN_IE_RF0LE;
    FDCANx->ILS |= FDCAN_ILS_TFEL;
    FDCANx->IE |= FDCAN_IE_TFEE;
    ret = fdcan_exit_init(FDCANx);
    llcan_irq_enable(FDCANx);
  }
  return ret;
}

void llcan_clear_send(FDCAN_GlobalTypeDef *FDCANx) {
  FDCANx->IR |= 0x3FCFFFFFU;
  bool ret = llcan_init(FDCANx);
  UNUSED(ret);
}
