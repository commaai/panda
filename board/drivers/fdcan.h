// for DLC
#define FDCAN_DLC_BYTES_0  ((uint32_t)0x00000000U) /*!< 0 bytes data field  */
#define FDCAN_DLC_BYTES_1  ((uint32_t)0x00010000U) /*!< 1 bytes data field  */
#define FDCAN_DLC_BYTES_2  ((uint32_t)0x00020000U) /*!< 2 bytes data field  */
#define FDCAN_DLC_BYTES_3  ((uint32_t)0x00030000U) /*!< 3 bytes data field  */
#define FDCAN_DLC_BYTES_4  ((uint32_t)0x00040000U) /*!< 4 bytes data field  */
#define FDCAN_DLC_BYTES_5  ((uint32_t)0x00050000U) /*!< 5 bytes data field  */
#define FDCAN_DLC_BYTES_6  ((uint32_t)0x00060000U) /*!< 6 bytes data field  */
#define FDCAN_DLC_BYTES_7  ((uint32_t)0x00070000U) /*!< 7 bytes data field  */
#define FDCAN_DLC_BYTES_8  ((uint32_t)0x00080000U) /*!< 8 bytes data field  */
#define FDCAN_DLC_BYTES_12 ((uint32_t)0x00090000U) /*!< 12 bytes data field */
#define FDCAN_DLC_BYTES_16 ((uint32_t)0x000A0000U) /*!< 16 bytes data field */
#define FDCAN_DLC_BYTES_20 ((uint32_t)0x000B0000U) /*!< 20 bytes data field */
#define FDCAN_DLC_BYTES_24 ((uint32_t)0x000C0000U) /*!< 24 bytes data field */
#define FDCAN_DLC_BYTES_32 ((uint32_t)0x000D0000U) /*!< 32 bytes data field */
#define FDCAN_DLC_BYTES_48 ((uint32_t)0x000E0000U) /*!< 48 bytes data field */
#define FDCAN_DLC_BYTES_64 ((uint32_t)0x000F0000U) /*!< 64 bytes data field */

/////////////////////

#define FDCAN_MESSAGE_RAM_SIZE 0x2800UL
#define FDCAN_START_ADDRESS 0x4000AC00UL
#define FDCAN_OFFSET 3412UL // bytes for each FDCAN module
#define FDCAN_OFFSET_W 853UL // words for each FDCAN module
#define FDCAN_END_ADDRESS 0x4000D3FCUL // Message RAM has a width of 4 Bytes

// Now is 500Kbit/s with 48MHz fdcan kernel clock
#define CAN_PRESCALER   6 
#define CAN_SYNC_JW     2 
#define CAN_PHASE_SEG1 11 
#define CAN_PHASE_SEG2  4

// RX FIFO 0
#define FDCAN_RX_FIFO_0_EL_CNT 32
#define FDCAN_RX_FIFO_0_HEAD_SIZE 8UL // bytes
#define FDCAN_RX_FIFO_0_DATA_SIZE 8UL // bytes
#define FDCAN_RX_FIFO_0_EL_SIZE (FDCAN_RX_FIFO_0_HEAD_SIZE + FDCAN_RX_FIFO_0_DATA_SIZE)
#define FDCAN_RX_FIFO_0_EL_W_SIZE (FDCAN_RX_FIFO_0_EL_SIZE / 4)
#define FDCAN_RX_FIFO_0_OFFSET 0UL

// TX FIFO
#define FDCAN_TX_FIFO_EL_CNT 32
#define FDCAN_TX_FIFO_HEAD_SIZE 8UL // bytes
#define FDCAN_TX_FIFO_DATA_SIZE 8UL // bytes
#define FDCAN_TX_FIFO_EL_SIZE (FDCAN_TX_FIFO_HEAD_SIZE + FDCAN_TX_FIFO_DATA_SIZE)
#define FDCAN_TX_FIFO_EL_W_SIZE (FDCAN_TX_FIFO_EL_SIZE / 4)
#define FDCAN_TX_FIFO_OFFSET (FDCAN_RX_FIFO_0_OFFSET + (FDCAN_RX_FIFO_0_EL_CNT * FDCAN_RX_FIFO_0_EL_W_SIZE))

// Helpers
FDCAN_GlobalTypeDef *fdcans[] = {FDCAN1, FDCAN2, FDCAN3};
uint8_t bus_lookup[] = {0,1,2};
uint8_t fdcan_num_lookup[] = {0,1,2,-1};
int8_t fdcan_forwarding[] = {-1,-1,-1,-1};
uint32_t fdcan_speed[] = {5000, 5000, 5000, 333};

#define FDCAN_MAX 3U
#define BUS_MAX 4U

#define CAN_BUS_RET_FLAG 0x80U
#define CAN_BUS_NUM_MASK 0x7FU

#define FDCANIF_FROM_FDCAN_NUM(num) (fdcans[num])
#define FDCAN_NUM_FROM_FDCANIF(FDCAN) ((FDCAN)==FDCAN1 ? 0 : ((FDCAN) == FDCAN2 ? 1 : 2))
#define BUS_NUM_FROM_FDCAN_NUM(num) (bus_lookup[num])
#define FDCAN_NUM_FROM_BUS_NUM(num) (fdcan_num_lookup[num])
#define FDCAN_NAME_FROM_FDCANIF(FDCAN_DEV) (((FDCAN_DEV)==FDCAN1) ? "FDCAN1" : (((FDCAN_DEV) == FDCAN2) ? "FDCAN2" : "FDCAN3"))

#define CAN_INIT_TIMEOUT_MS 500U


// CAN message structure bits
#define CAN_STANDARD_FORMAT 0UL
#define CAN_EXTENDED_FORMAT 1UL

#define DATA_FRAME 0UL
#define REMOTE_FRAME 1UL


////////////////////////////////
// For tests, remove later, REDEBUG
typedef struct {
  uint8_t error;
  uint32_t id;
  uint8_t data[8];
  uint8_t length;
  uint8_t format;
  uint8_t type;
} can_message;

////////////////
// For backwards compatibility with safety code
typedef struct {
  uint32_t RIR;  /*!< CAN receive FIFO mailbox identifier register */
  uint32_t RDTR; /*!< CAN receive FIFO mailbox data length control and time stamp register */
  uint32_t RDLR; /*!< CAN receive FIFO mailbox data low register */
  uint32_t RDHR; /*!< CAN receive FIFO mailbox data high register */
} CAN_FIFOMailBox_TypeDef;
////////////////

// global CAN stats
int can_rx_cnt = 0;
int can_tx_cnt = 0;
int can_txd_cnt = 0;
int can_err_cnt = 0;
int can_overflow_cnt = 0;

uint32_t can_rx_errs = 0;
uint32_t can_send_errs = 0;
uint32_t can_fwd_errs = 0;
uint32_t gmlan_send_errs = 0;

can_message can_rx_message;



void fdcan_send_msg(can_message *msg, uint8_t can_number) {
  can_tx_cnt++;

  FDCAN_GlobalTypeDef *FDCANx = FDCANIF_FROM_FDCAN_NUM(can_number);
  uint32_t TxFIFOSA = FDCAN_START_ADDRESS + (can_number * FDCAN_OFFSET) + (FDCAN_RX_FIFO_0_EL_CNT * FDCAN_RX_FIFO_0_EL_SIZE);
  CAN_FIFOMailBox_TypeDef *fifo;
	uint8_t tx_index;
	
  // check buffer
	if ((FDCANx->TXFQS & FDCAN_TXFQS_TFQF) != 0) {
		// buffer full, do something!!
	}
	
  // get nex free index (from 0 to 31)
	tx_index = (FDCANx->TXFQS >> FDCAN_TXFQS_TFQPI_Pos) & 0x1F; 

  // getting fifo buffer address
  fifo = (CAN_FIFOMailBox_TypeDef *)(TxFIFOSA + tx_index * FDCAN_TX_FIFO_EL_SIZE);
	
  //msg id format
	if (msg->format == CAN_STANDARD_FORMAT) {
		fifo->RIR = (msg->id << 18);
	} else {
		fifo->RIR = msg->id;
		fifo->RIR |= 1UL << 30; // extended flag
	}
	
  // remote frame
	if (msg->type == REMOTE_FRAME) fifo->RIR |= 1UL << 29;
	
  // data size
	fifo->RDTR = (8UL << 16);  //Data size
  
  // data
	fifo->RDLR = (msg->data[3] << 24)|(msg->data[2] << 16)|(msg->data[1] << 8)|msg->data[0];
	fifo->RDHR = (msg->data[7] << 24)|(msg->data[6] << 16)|(msg->data[5] << 8)|msg->data[4];

  // increment index and transmit
	FDCANx->TXBAR |= (1UL << tx_index);   
}






void fdcan_read_msg(can_message* msg, uint8_t idx, uint8_t can_number) {
  can_rx_cnt++;
  
  uint32_t RxFIFO0SA = FDCAN_START_ADDRESS + (can_number * FDCAN_OFFSET);
  CAN_FIFOMailBox_TypeDef *fifo;

  // getting address
  fifo = (CAN_FIFOMailBox_TypeDef *)(RxFIFO0SA + idx * FDCAN_RX_FIFO_0_EL_SIZE);

  // parse fields
	msg->error   = (uint8_t)((fifo->RIR >> 31) & 0x01);
	msg->format  = (uint8_t)((fifo->RIR >> 30) & 0x01);
	msg->type    = (uint8_t)((fifo->RIR >> 29) & 0x01);
	
  // message id type
	if (msg->format == CAN_EXTENDED_FORMAT) {
		msg->id = fifo->RIR & 0x1FFFFFFF;
	} else {
		msg->id = (fifo->RIR >> 18) & 0x7FF;
	}
	
  // data length
	msg->length  = (uint8_t)((fifo->RDTR >> 16) & 0x0F);
	
  // data
	msg->data[0] = (uint8_t)((fifo->RDLR >>  0) & 0xFF);
	msg->data[1] = (uint8_t)((fifo->RDLR >>  8) & 0xFF);
	msg->data[2] = (uint8_t)((fifo->RDLR >> 16) & 0xFF);
	msg->data[3] = (uint8_t)((fifo->RDLR >> 24) & 0xFF);
	
	msg->data[4] = (uint8_t)((fifo->RDHR >>  0) & 0xFF);
	msg->data[5] = (uint8_t)((fifo->RDHR >>  8) & 0xFF);
	msg->data[6] = (uint8_t)((fifo->RDHR >> 16) & 0xFF);
	msg->data[7] = (uint8_t)((fifo->RDHR >> 24) & 0xFF);


    //REDEBUG
    for (uint8_t i=0;i<8;i++){
      if (msg->data[i] == i*10) { 
        for (uint8_t cnt=0;cnt<1;cnt++) {
            switch(can_number) {
              case 0:
                delay_ms(50);
                register_clear_bits(&(GPIOE->ODR), (1U << 2));

                delay_ms(50);
                register_set_bits(&(GPIOE->ODR), (1U << 2));
              break;

              case 1:
                delay_ms(50);
                register_clear_bits(&(GPIOE->ODR), (1U << 3));

                delay_ms(50);
                register_set_bits(&(GPIOE->ODR), (1U << 3));
              break;

              case 2:
                delay_ms(50);
                register_clear_bits(&(GPIOE->ODR), (1U << 4));

                delay_ms(50);
                register_set_bits(&(GPIOE->ODR), (1U << 4));
              break;
            }
        }
      } 
    }
    //REDEBUG
}







void fdcan_irq_handler(uint8_t can_number) {
    FDCAN_GlobalTypeDef *FDCANx = FDCANIF_FROM_FDCAN_NUM(can_number);
	uint8_t rx_fifo_get_index;
   
	// got new message
	if((FDCANx->IR & FDCAN_IR_RF0N) != 0) {
    // clear flag
		FDCANx->IR = FDCAN_IR_RF0N; 
    
    // getting new message index (0 to 63)
		rx_fifo_get_index = (uint8_t)((FDCANx->RXF0S >> FDCAN_RXF0S_F0GI_Pos) & 0x3F);
    
    // reading message from fifo
		fdcan_read_msg(&can_rx_message, rx_fifo_get_index, can_number);
    
    // update read index 
		FDCANx->RXF0A = rx_fifo_get_index;
	};
	
	// msg lost if fifo was full (think how to handle...)
	if((FDCANx->IR & FDCAN_IR_RF0L) != 0) {
    // clear flag
		FDCANx->IR = FDCAN_IR_RF0L; 
	};
	
	// buffer RX FIFO full
	if((FDCANx->IR & FDCAN_IR_RF0F) != 0) {
    // do something
  };
}








void FDCAN1_IT0_IRQ_Handler(void) { fdcan_irq_handler(0); }
//void CAN1_RX0_IRQ_Handler(void) { can_rx(0); }
//void CAN1_SCE_IRQ_Handler(void) { can_sce(CAN1); }

void FDCAN2_IT0_IRQ_Handler(void) { fdcan_irq_handler(1); }
//void CAN2_RX0_IRQ_Handler(void) { can_rx(1); }
//void CAN2_SCE_IRQ_Handler(void) { can_sce(CAN2); }

void FDCAN3_IT0_IRQ_Handler(void) { fdcan_irq_handler(2); }
//void CAN3_RX0_IRQ_Handler(void) { can_rx(2); }
//void CAN3_SCE_IRQ_Handler(void) { can_sce(CAN3); }








bool llfdcan_init(FDCAN_GlobalTypeDef *FDCANx) {
  bool ret = true;
  uint32_t can_number = FDCAN_NUM_FROM_FDCANIF(FDCANx);

  // Exit from sleep mode
  FDCANx->CCCR &= ~(FDCAN_CCCR_CSR);
  while((FDCANx->CCCR & FDCAN_CCCR_CSA) == FDCAN_CCCR_CSA);

  // Request init
  uint32_t timeout_counter = 0U;
  FDCANx->CCCR |= FDCAN_CCCR_INIT;
  while((FDCANx->CCCR & FDCAN_CCCR_INIT) == 0) {
    delay_ms(1);
    timeout_counter++;

    if(timeout_counter >= CAN_INIT_TIMEOUT_MS) {
      puts(FDCAN_NAME_FROM_FDCANIF(FDCANx)); puts(" initialization timed out!\n");
      ret = false;
      break;
    }
  }

  if(ret) {

    // Enable config change
    FDCANx->CCCR |= FDCAN_CCCR_CCE;

    // Disable automatic retransmission of failed messages
    FDCANx->CCCR |= FDCAN_CCCR_DAR; // Imho should be enabled later?

    // Disable transmission pause feature
    FDCANx->CCCR &= ~(FDCAN_CCCR_TXP);

    // Disable protocol exception handling
    FDCANx->CCCR |= FDCAN_CCCR_PXHD;

    // Set FDCAN frame format (REDEBUG)
        //  Classic mode
    FDCANx->CCCR &= ~(FDCAN_CCCR_BRSE);
    FDCANx->CCCR &= ~(FDCAN_CCCR_FDOE);
        // FD without BRS
    //FDCANx->CCCR |= FDCAN_CCCR_FDOE;
        // FD with BRS
    //FDCANx->CCCR |= (FDCAN_CCCR_FDOE | FDCAN_CCCR_BRSE);



        /* Set FDCAN Operating Mode:
                | Normal | Restricted |    Bus     | Internal | External
                |        | Operation  | Monitoring | LoopBack | LoopBack
        CCCR.TEST |   0    |     0      |     0      |    1     |    1
        CCCR.MON  |   0    |     0      |     1      |    1     |    0
        TEST.LBCK |   0    |     0      |     0      |    1     |    1
        CCCR.ASM  |   0    |     1      |     0      |    0     |    0
        */
    FDCANx->CCCR &= ~(FDCAN_CCCR_TEST);
    FDCANx->TEST &= ~(FDCAN_TEST_LBCK);
    FDCANx->CCCR &= ~(FDCAN_CCCR_MON);
    FDCANx->CCCR &= ~(FDCAN_CCCR_ASM);

    // //REDEBUG - external loopback mode enable
    FDCANx->CCCR |= FDCAN_CCCR_TEST;
    FDCANx->TEST |= FDCAN_TEST_LBCK;
    FDCANx->CCCR &= ~(FDCAN_CCCR_MON);
    FDCANx->CCCR &= ~(FDCAN_CCCR_ASM);
    
    //REDEBUG

    // Set the nominal bit timing register
    FDCANx->NBTP = ((CAN_SYNC_JW-1)<<FDCAN_NBTP_NSJW_Pos) | ((CAN_PHASE_SEG1-1)<<FDCAN_NBTP_NTSEG1_Pos) | ((CAN_PHASE_SEG2-1)<<FDCAN_NBTP_NTSEG2_Pos) | ((CAN_PRESCALER-1)<<FDCAN_NBTP_NBRP_Pos);

    // If FD with BRS enabled - set data bit timing register
    //FDCANx->DBTP = ((CAN_SYNC_JW-1)<<FDCAN_DBTP_DSJW_Pos) | ((CAN_PHASE_SEG1-1)<<FDCAN_DBTP_DTSEG1_Pos) | ((CAN_PHASE_SEG2-1)<<FDCAN_DBTP_DTSEG2_Pos) | ((CAN_PRESCALER-1)<<FDCAN_DBTP_DBRP_Pos);

    // Set TX mode to FIFO
    FDCANx->TXBC &= ~(FDCAN_TXBC_TFQM);

    // Configure TX element size (for now 8 bytes, no need to change)
    //FDCANx->TXESC |= 0x000U;

    //Configure RX FIFO0, FIFO1, RX buffer element sizes (no need for now, using classic 8 bytes)
    register_set(&(FDCANx->RXESC), 0x0U, (FDCAN_RXESC_F0DS | FDCAN_RXESC_F1DS | FDCAN_RXESC_RBDS));

    // Disable TT for FDCAN1
    //if (FDCANx == FDCAN1) {
     //   FDCANx->TTOCF &= ~(FDCAN_TTOCF_OM);
    //}

    // Disable filtering, accept all valid frames received
    FDCANx->XIDFC &= ~(FDCAN_XIDFC_LSE); // No extended filters
    FDCANx->SIDFC &= ~(FDCAN_SIDFC_LSS); // No standard filters
    FDCANx->GFC &= ~(FDCAN_GFC_RRFE); // Accept extended remote frames
    FDCANx->GFC &= ~(FDCAN_GFC_RRFS); // Accept standard remote frames
    FDCANx->GFC &= ~(FDCAN_GFC_ANFE); // Accept extended frames to FIFO 0
    FDCANx->GFC &= ~(FDCAN_GFC_ANFS); // Accept standard frames to FIFO 0

    uint32_t RxFIFO0SA = FDCAN_START_ADDRESS + (can_number * FDCAN_OFFSET);
    uint32_t TxFIFOSA = RxFIFO0SA + (FDCAN_RX_FIFO_0_EL_CNT * FDCAN_RX_FIFO_0_EL_SIZE);

    // RX FIFO 0
    FDCANx->RXF0C = (FDCAN_RX_FIFO_0_OFFSET + (can_number * FDCAN_OFFSET_W)) << FDCAN_RXF0C_F0SA_Pos;
    FDCANx->RXF0C |= FDCAN_RX_FIFO_0_EL_CNT << FDCAN_RXF0C_F0S_Pos;

    // TX FIFO (mode set earlier)
    FDCANx->TXBC = (FDCAN_TX_FIFO_OFFSET + (can_number * FDCAN_OFFSET_W)) << FDCAN_TXBC_TBSA_Pos;
    FDCANx->TXBC |= FDCAN_TX_FIFO_EL_CNT << FDCAN_TXBC_TFQS_Pos;

    // Flush allocated RAM
    uint32_t EndAddress = TxFIFOSA + (FDCAN_TX_FIFO_EL_CNT * FDCAN_TX_FIFO_EL_SIZE);
    for (uint32_t RAMcounter = RxFIFO0SA; RAMcounter < EndAddress; RAMcounter += 4U) {
        *(uint32_t *)(RAMcounter) = 0x00000000;
    }

    // Turn on interrupt on RX
    FDCANx->IE |= FDCAN_IE_RF0NE;
    FDCANx->ILE |= FDCAN_ILE_EINT0;

    // Request leave init, start FDCAN
    FDCANx->CCCR &= ~(FDCAN_CCCR_INIT);
    while((FDCANx->CCCR & FDCAN_CCCR_INIT) == 1);

    if (FDCANx == FDCAN1) {
      NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
    } else if (FDCANx == FDCAN2) {
      NVIC_EnableIRQ(FDCAN2_IT0_IRQn);
    //#ifdef CAN3 //REDEBUG
      } else if (FDCANx == FDCAN3) {
        NVIC_EnableIRQ(FDCAN3_IT0_IRQn);
    //#endif
    } else {
      puts("Invalid CAN: initialization failed\n");
    }

  }
  return ret;
}








bool fdcan_init(uint8_t can_number) {
  bool ret = false;

  REGISTER_INTERRUPT(FDCAN1_IT0_IRQn, FDCAN1_IT0_IRQ_Handler, CAN_INTERRUPT_RATE, FAULT_INTERRUPT_RATE_CAN_1)
  REGISTER_INTERRUPT(FDCAN2_IT0_IRQn, FDCAN2_IT0_IRQ_Handler, CAN_INTERRUPT_RATE, FAULT_INTERRUPT_RATE_CAN_2)
  REGISTER_INTERRUPT(FDCAN3_IT0_IRQn, FDCAN3_IT0_IRQ_Handler, CAN_INTERRUPT_RATE, FAULT_INTERRUPT_RATE_CAN_3)

  if (can_number != 0xffU) {
    FDCAN_GlobalTypeDef *FDCANx = FDCANIF_FROM_FDCAN_NUM(can_number);
    //ret &= can_set_speed(can_number); // Maybe better to set speed while initializing? 
    ret &= llfdcan_init(FDCANx);
    // in case there are queued up messages
    //process_can(can_number); //Queued messages from who? 
  }
  return ret;
}





void fdcan_init_all(void) {
  bool ret = true;
  for (uint8_t i=0U; i < FDCAN_MAX; i++) {
    //can_clear(can_queues[i]); //REDEBUG check this call
    ret &= fdcan_init(i);
  }
  //UNUSED(ret);
}

//////////////////
///////////////////
//////////////////

void can_set_obd(uint8_t harness_orientation, bool obd){
  if(obd){
    puts("setting CAN2 to be OBD\n");
  } else {
    puts("setting CAN2 to be normal\n");
  }
  if(board_has_obd()){
    if(obd != (bool)(harness_orientation == HARNESS_STATUS_NORMAL)){
        // B5,B6: disable normal mode
        set_gpio_pullup(GPIOB, 5, PULL_NONE);
        set_gpio_mode(GPIOB, 5, MODE_ANALOG);

        set_gpio_pullup(GPIOB, 6, PULL_NONE);
        set_gpio_mode(GPIOB, 6, MODE_ANALOG);
        // B12,B13: FDCAN2 mode
        set_gpio_output_type(GPIOB, 12, OUTPUT_TYPE_PUSH_PULL);
        set_gpio_pullup(GPIOB, 12, PULL_NONE);
        set_gpio_speed(GPIOB, 12, SPEED_LOW);
        set_gpio_alternate(GPIOB, 12, GPIO_AF9_FDCAN2);

        set_gpio_output_type(GPIOB, 13, OUTPUT_TYPE_PUSH_PULL);
        set_gpio_pullup(GPIOB, 13, PULL_NONE);
        set_gpio_speed(GPIOB, 13, SPEED_LOW);
        set_gpio_alternate(GPIOB, 13, GPIO_AF9_FDCAN2);
    } else {
        // B5,B6: FDCAN2 mode
        set_gpio_output_type(GPIOB, 5, OUTPUT_TYPE_PUSH_PULL);
        set_gpio_pullup(GPIOB, 5, PULL_NONE);
        set_gpio_speed(GPIOB, 5, SPEED_LOW);
        set_gpio_alternate(GPIOB, 5, GPIO_AF9_FDCAN2);

        set_gpio_output_type(GPIOB, 6, OUTPUT_TYPE_PUSH_PULL);
        set_gpio_pullup(GPIOB, 6, PULL_NONE);
        set_gpio_speed(GPIOB, 6, SPEED_LOW);
        set_gpio_alternate(GPIOB, 6, GPIO_AF9_FDCAN2);
        // B12,B13: disable normal mode
        set_gpio_pullup(GPIOB, 12, PULL_NONE);
        set_gpio_mode(GPIOB, 12, MODE_ANALOG);

        set_gpio_pullup(GPIOB, 13, PULL_NONE);
        set_gpio_mode(GPIOB, 13, MODE_ANALOG);
    }
  } else {
    puts("OBD CAN not available on this board\n");
  }
}