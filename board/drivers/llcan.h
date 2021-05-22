// this is needed for 1 mbps support
#define CAN_QUANTA 8U
#define CAN_SEQ1 6 // roundf(quanta * 0.875f) - 1;
#define CAN_SEQ2 1 // roundf(quanta * 0.125f);

#define CAN_PCLK 24000U
// 333 = 33.3 kbps
// 5000 = 500 kbps
#define can_speed_to_prescaler(x) (CAN_PCLK / CAN_QUANTA * 10U / (x))

#define GET_BUS(msg) (((msg)->RDTR >> 4) & 0xFF)
#define GET_LEN(msg) ((msg)->RDTR & 0xF)
#define GET_ADDR(msg) ((((msg)->RIR & 4) != 0) ? ((msg)->RIR >> 3) : ((msg)->RIR >> 21))
#define GET_BYTE(msg, b) (((int)(b) > 3) ? (((msg)->RDHR >> (8U * ((unsigned int)(b) % 4U))) & 0xFFU) : (((msg)->RDLR >> (8U * (unsigned int)(b))) & 0xFFU))
#define GET_BYTES_04(msg) ((msg)->RDLR)
#define GET_BYTES_48(msg) ((msg)->RDHR)
#define GET_FLAG(value, mask) (((__typeof__(mask))param & mask) == mask)

#define CAN_INIT_TIMEOUT_MS 500U
#define CAN_NAME_FROM_CANIF(CAN_DEV) (((CAN_DEV)==CAN1) ? "CAN1" : (((CAN_DEV) == CAN2) ? "CAN2" : "CAN3"))

void puts(const char *a);

bool llcan_set_speed(CAN_TypeDef *CAN_obj, uint32_t speed, bool loopback, bool silent) {
  bool ret = true;

  // initialization mode
  register_set(&(CAN_obj->MCR), CAN_MCR_TTCM | CAN_MCR_INRQ, 0x180FFU);
  uint32_t timeout_counter = 0U;
  while((CAN_obj->MSR & CAN_MSR_INAK) != CAN_MSR_INAK){
    // Delay for about 1ms
    delay(10000);
    timeout_counter++;

    if(timeout_counter >= CAN_INIT_TIMEOUT_MS){
      puts(CAN_NAME_FROM_CANIF(CAN_obj)); puts(" set_speed timed out (1)!\n");
      ret = false;
      break;
    }
  }

  if(ret){
    // set time quanta from defines
    register_set(&(CAN_obj->BTR), ((CAN_BTR_TS1_0 * (CAN_SEQ1-1)) |
              (CAN_BTR_TS2_0 * (CAN_SEQ2-1)) |
              (can_speed_to_prescaler(speed) - 1U)), 0xC37F03FFU);

    // silent loopback mode for debugging
    if (loopback) {
      register_set_bits(&(CAN_obj->BTR), CAN_BTR_SILM | CAN_BTR_LBKM);
    }
    if (silent) {
      register_set_bits(&(CAN_obj->BTR), CAN_BTR_SILM);
    }

    // reset
    register_set(&(CAN_obj->MCR), CAN_MCR_TTCM | CAN_MCR_ABOM, 0x180FFU);

    timeout_counter = 0U;
    while(((CAN_obj->MSR & CAN_MSR_INAK) == CAN_MSR_INAK)) {
      // Delay for about 1ms
      delay(10000);
      timeout_counter++;

      if(timeout_counter >= CAN_INIT_TIMEOUT_MS){
        puts(CAN_NAME_FROM_CANIF(CAN_obj)); puts(" set_speed timed out (2)!\n");
        ret = false;
        break;
      }
    }
  }

  return ret;
}

#define FDCAN_MESSAGE_RAM_SIZE 0x2800U
#define FDCAN_MESSAGE_RAM_END_ADDRESS (SRAMCAN_BASE + FDCAN_MESSAGE_RAM_SIZE - 0x4U) /* The Message RAM has a width of 4 Bytes */

#define FDCAN_DATA_BYTES_8  ((uint32_t)0x00000004U) /*!< 8 bytes data field  */
#define FDCAN_DATA_BYTES_12 ((uint32_t)0x00000005U) /*!< 12 bytes data field */
#define FDCAN_DATA_BYTES_16 ((uint32_t)0x00000006U) /*!< 16 bytes data field */
#define FDCAN_DATA_BYTES_20 ((uint32_t)0x00000007U) /*!< 20 bytes data field */
#define FDCAN_DATA_BYTES_24 ((uint32_t)0x00000008U) /*!< 24 bytes data field */
#define FDCAN_DATA_BYTES_32 ((uint32_t)0x0000000AU) /*!< 32 bytes data field */
#define FDCAN_DATA_BYTES_48 ((uint32_t)0x0000000EU) /*!< 48 bytes data field */
#define FDCAN_DATA_BYTES_64 ((uint32_t)0x00000012U) /*!< 64 bytes data field */

bool llcan_init(CAN_TypeDef *CAN_obj) {
  bool ret = true;

  ////////////////////////////////////////////////
  ////////////////////////////////////////////////
  ////////////////////////////////////////////////

  // Exit from sleep mode
  CAN_obj->CCCR &= ~(FDCAN_CCCR_CSR);
  while((CAN_obj->CCCR & FDCAN_CCCR_CSA) == FDCAN_CCCR_CSA);

  // Request init
  CAN_obj->CCCR |= FDCAN_CCCR_INIT;
  while((CAN_obj->CCCR & FDCAN_CCCR_INIT) == 0U);

  // Enable config change
  CAN_obj->CCCR |= FDCAN_CCCR_CCE;

  // Disable automatic retransmission of failed messages
  CAN_obj->CCCR |= FDCAN_CCCR_DAR; // Imho should be enabled later

  // Disable transmission pause feature
  CAN_obj->CCCR &= ~(FDCAN_CCCR_TXP);

  // Disable protocol exception handling
  CAN_obj->CCCR |= FDCAN_CCCR_PXHD;

  // Set FDCAN frame format (REDEBUG)
    //  Classic mode
  FDCCAN_objAN->CCCR &= ~(FDCAN_CCCR_BRSE);
  CAN_obj->CCCR &= ~(FDCAN_CCCR_FDOE);
    // FD without BRS
  //CAN_obj->CCCR |= FDCAN_CCCR_FDOE;
    // FD with BRS
  //CAN_obj->CCCR |= (FDCAN_CCCR_FDOE | FDCAN_CCCR_BRSE);

  // Reset FDCAN operation mode
  CAN_obj->CCCR &= ~(FDCAN_CCCR_TEST | FDCAN_CCCR_MON | FDCAN_CCCR_ASM);
  CAN_obj->TEST &= ~(FDCAN_TEST_LBCK);

    /* Set FDCAN Operating Mode:
               | Normal | Restricted |    Bus     | Internal | External
               |        | Operation  | Monitoring | LoopBack | LoopBack
     CCCR.TEST |   0    |     0      |     0      |    1     |    1
     CCCR.MON  |   0    |     0      |     1      |    1     |    0
     TEST.LBCK |   0    |     0      |     0      |    1     |    1
     CCCR.ASM  |   0    |     1      |     0      |    0     |    0
  */

  // Set the nominal bit timing register
  CAN_obj->NBTP = (0U<<FDCAN_NBTP_NSJW_Pos) | (1U<<FDCAN_NBTP_NTSEG1_Pos) | (1U<<FDCAN_NBTP_NTSEG2_Pos) | (0U<<FDCAN_NBTP_NBRP_Pos);

  // If FD with BRS enabled - set data bit timing register
  //CAN_obj->DBTP = (0U<<FDCAN_DBTP_DSJW_Pos) | (0U<<FDCAN_DBTP_DTSEG1_Pos) | (0U<<FDCAN_DBTP_DTSEG2_Pos) | (0U<<FDCAN_DBTP_DBRP_Pos);

  // Set TX mode to FIFO
  CAN_obj->TXBC &= ~(FDCAN_TXBC_TFQM);

  // Configure TX element size (for now 8 bytes, no need to change)
  //CAN_obj->TXESC |= 0x000U;

  //Configure RX FIFO0, FIFO1, RX buffer element sizes (no need for now, using classic 8 bytes)
  register_Set(&(CAN_obj->RXESC), 0x0U, (FDCAN_RXESC_F0DS | FDCAN_RXESC_F1DS | FDCAN_RXESC_RBDS));

  // Disable TT for FDCAN1
  if (CAN_obj == CAN1) {
    CAN_obj->TTOCF &= ~(FDCAN_TTOCF_OM);
  }

  // Need to calculate RAM block addresses, so need to keep track of offset as a start address. Will need to add to input params.
  // At this time 0 standard and 0 extended filters, so ignore those lists.
  // This will be changed, for each FDCANx personal offset. (10kb RAM total for 3 FDCAN modules, so 3412 bytes per module for everything)
  // or 2560 words total, ~852 per module
  uint32_t StartAddress;

  StartAddress = 0; // for FDCAN1
  //StartAddress = 852; // for FDCAN2
  //StartAddress = 1704; // for FDCAN3

  // RX FIFO 0 start address
  register_set(&(CAN_obj->RXF0C), (StartAddress<<FDCAN_RXF0C_F0SA_Pos) , FDCAN_RXF0C_F0SA);
  // RX FIFO 0 elements number
  register_set(&(CAN_obj->RXF0C), (32U<<FDCAN_RXF0C_F0S_Pos) , FDCAN_RXF0C_F0S); // 32 RX elements
  uint32_t RxFIFO0SA = SRAMCAN_BASE + (StartAddress * 4U);

  // TX buffer list start address
  StartAddress += (32U * FDCAN_DATA_BYTES_8) 
  register_set(&(CAN_obj->TXBC), (StartAddress<<FDCAN_TXBC_TBSA_Pos) , FDCAN_TXBC_TBSA);
  // TX FIFO elements number
  register_set(&(CAN_obj->TXBC), (32U<<FDCAN_TXBC_TFQS_Pos) , FDCAN_TXBC_TFQS); // 32 TX elements
  uint32_t TxFIFOQSA = RxFIFO0SA + (32U * FDCAN_DATA_BYTES_8 * 4U);
  
  uint32_t EndAddress = TxFIFOQSA + (32U * FDCAN_DATA_BYTES_8 * 4U);

  // Flush allocated RAM
  for (uint32_t RAMcounter = RxFIFO0SA; RAMcounter < EndAddress; RAMcounter += 4U) {
    *(uint32_t *)(RAMcounter) = 0x00000000;
  }

  //////////

  // Request leave init, start FDCAN
  CAN_obj->CCCR &= ~(FDCAN_CCCR_INIT);

  ////////////////////////////////////////////////
  ////////////////////////////////////////////////
  ////////////////////////////////////////////////

  // Enter init mode
  register_set_bits(&(CAN_obj->FMR), CAN_FMR_FINIT);

  // Wait for INAK bit to be set
  uint32_t timeout_counter = 0U;
  while(((CAN_obj->MSR & CAN_MSR_INAK) == CAN_MSR_INAK)) {
    // Delay for about 1ms
    delay(10000);
    timeout_counter++;

    if(timeout_counter >= CAN_INIT_TIMEOUT_MS){
      puts(CAN_NAME_FROM_CANIF(CAN_obj)); puts(" initialization timed out!\n");
      ret = false;
      break;
    }
  }

  if(ret){
    // no mask
    // For some weird reason some of these registers do not want to set properly on CAN2 and CAN3. Probably something to do with the single/dual mode and their different filters.
    CAN_obj->sFilterRegister[0].FR1 = 0U;
    CAN_obj->sFilterRegister[0].FR2 = 0U;
    CAN_obj->sFilterRegister[14].FR1 = 0U;
    CAN_obj->sFilterRegister[14].FR2 = 0U;
    CAN_obj->FA1R |= 1U | (1U << 14);

    // Exit init mode, do not wait
    register_clear_bits(&(CAN_obj->FMR), CAN_FMR_FINIT);

    // enable certain CAN interrupts
    register_set_bits(&(CAN_obj->IER), CAN_IER_TMEIE | CAN_IER_FMPIE0 |  CAN_IER_WKUIE);

    if (CAN_obj == CAN1) {
      NVIC_EnableIRQ(CAN1_TX_IRQn);
      NVIC_EnableIRQ(CAN1_RX0_IRQn);
      NVIC_EnableIRQ(CAN1_SCE_IRQn);
    } else if (CAN_obj == CAN2) {
      NVIC_EnableIRQ(CAN2_TX_IRQn);
      NVIC_EnableIRQ(CAN2_RX0_IRQn);
      NVIC_EnableIRQ(CAN2_SCE_IRQn);
    #ifdef CAN3
      } else if (CAN_obj == CAN3) {
        NVIC_EnableIRQ(CAN3_TX_IRQn);
        NVIC_EnableIRQ(CAN3_RX0_IRQn);
        NVIC_EnableIRQ(CAN3_SCE_IRQn);
    #endif
    } else {
      puts("Invalid CAN: initialization failed\n");
    }
  }
  return ret;
}

void llcan_clear_send(CAN_TypeDef *CAN_obj) {
  CAN_obj->TSR |= CAN_TSR_ABRQ0;
  register_clear_bits(&(CAN_obj->MSR), CAN_MSR_ERRI);
  // cppcheck-suppress selfAssignment ; needed to clear the register
  CAN_obj->MSR = CAN_obj->MSR;
}

