

////////////////


typedef struct {
  volatile uint32_t w_ptr;
  volatile uint32_t r_ptr;
  uint32_t fifo_size;
  CAN_FIFOMailBox_TypeDef *elems;
} can_ring;
////////////////

// must reinit after changing these
extern int can_loopback, can_silent;
extern uint32_t can_speed[4];

// Ignition detected from CAN meessages
bool ignition_can = false;
bool ignition_cadillac = false;
uint32_t ignition_can_cnt = 0U;

// end API

#define ALL_CAN_SILENT 0xFF
#define ALL_CAN_LIVE 0

// Helpers
FDCAN_GlobalTypeDef *fdcans[] = {FDCAN1, FDCAN2, FDCAN3};
uint8_t bus_lookup[] = {0,1,2};
uint8_t fdcan_num_lookup[] = {0,1,2,-1};
int8_t fdcan_forwarding[] = {-1,-1,-1,-1};
uint32_t can_speed[] = {5000, 5000, 5000, 333};

#define can_buffer(x, size) \
  CAN_FIFOMailBox_TypeDef elems_##x[size]; \
  can_ring can_##x = { .w_ptr = 0, .r_ptr = 0, .fifo_size = size, .elems = (CAN_FIFOMailBox_TypeDef *)&elems_##x };

can_buffer(rx_q, 0x1000)
can_buffer(tx1_q, 0x100)
can_buffer(tx2_q, 0x100)
can_buffer(tx3_q, 0x100)
can_buffer(txgmlan_q, 0x100)
can_ring *can_queues[] = {&can_tx1_q, &can_tx2_q, &can_tx3_q, &can_txgmlan_q};

// global CAN stats
int can_rx_cnt = 0;
int can_tx_cnt = 0;
int can_txd_cnt = 0;
int can_err_cnt = 0;
int can_overflow_cnt = 0;

// Prototypes
bool fdcan_init(uint8_t can_number);

bool can_pop(can_ring *q, CAN_FIFOMailBox_TypeDef *elem) {
  bool ret = false;

  ENTER_CRITICAL();
  if (q->w_ptr != q->r_ptr) {
    *elem = q->elems[q->r_ptr];
    if ((q->r_ptr + 1U) == q->fifo_size) {
      q->r_ptr = 0;
    } else {
      q->r_ptr += 1U;
    }
    ret = true;
  }
  EXIT_CRITICAL();

  return ret;
}

bool can_push(can_ring *q, CAN_FIFOMailBox_TypeDef *elem) {
  bool ret = false;
  uint32_t next_w_ptr;

  ENTER_CRITICAL();
  if ((q->w_ptr + 1U) == q->fifo_size) {
    next_w_ptr = 0;
  } else {
    next_w_ptr = q->w_ptr + 1U;
  }
  if (next_w_ptr != q->r_ptr) {
    q->elems[q->w_ptr] = *elem;
    q->w_ptr = next_w_ptr;
    ret = true;
  }
  EXIT_CRITICAL();
  if (!ret) {
    can_overflow_cnt++;
    #ifdef DEBUG
      puts("can_push failed! buffer overflow.\n");
    #endif
  }
  return ret;
}

uint32_t can_slots_empty(can_ring *q) {
  uint32_t ret = 0;

  ENTER_CRITICAL();
  if (q->w_ptr >= q->r_ptr) {
    ret = q->fifo_size - 1U - q->w_ptr + q->r_ptr;
  } else {
    ret = q->r_ptr - q->w_ptr - 1U;
  }
  EXIT_CRITICAL();

  return ret;
}

void can_clear(can_ring *q) {
  ENTER_CRITICAL();
  q->w_ptr = 0;
  q->r_ptr = 0;
  EXIT_CRITICAL();
}

#define FDCAN_MAX 3U
#define BUS_MAX 4U

#define CAN_BUS_RET_FLAG 0x80U
#define CAN_BUS_NUM_MASK 0x7FU

#define FDCANIF_FROM_FDCAN_NUM(num) (fdcans[num])

#define BUS_NUM_FROM_FDCAN_NUM(num) (bus_lookup[num])
#define FDCAN_NUM_FROM_BUS_NUM(num) (fdcan_num_lookup[num])


// CAN message structure bits
#define CAN_STANDARD_FORMAT 0UL
#define CAN_EXTENDED_FORMAT 1UL

#define DATA_FRAME 0UL
#define REMOTE_FRAME 1UL


extern int can_live, pending_can_live;

int can_live = 0, pending_can_live = 0, can_loopback = 0, can_silent = ALL_CAN_SILENT;

uint32_t can_rx_errs = 0;
uint32_t can_send_errs = 0;
uint32_t can_fwd_errs = 0;
uint32_t gmlan_send_errs = 0;





void fdcan_flip_buses(uint8_t bus1, uint8_t bus2){
  bus_lookup[bus1] = bus2;
  bus_lookup[bus2] = bus1;
  fdcan_num_lookup[bus1] = bus2;
  fdcan_num_lookup[bus2] = bus1;
}




void can_set_gmlan(uint8_t bus) {
  UNUSED(bus);
  puts("GMLAN not available on red panda\n");
}



void ignition_can_hook(CAN_FIFOMailBox_TypeDef *to_push) {
  int bus = GET_BUS(to_push);
  int addr = GET_ADDR(to_push);
  int len = GET_LEN(to_push);

  ignition_can_cnt = 0U;  // reset counter

  if (bus == 0) {
    // TODO: verify on all supported GM models that we can reliably detect ignition using only this signal,
    // since the 0x1F1 signal can briefly go low immediately after ignition
    if ((addr == 0x160) && (len == 5)) {
      // this message isn't all zeros when ignition is on
      ignition_cadillac = GET_BYTES_04(to_push) != 0;
    }
    // GM exception
    if ((addr == 0x1F1) && (len == 8)) {
      // Bit 5 is ignition "on"
      bool ignition_gm = ((GET_BYTE(to_push, 0) & 0x20) != 0);
      ignition_can = ignition_gm || ignition_cadillac;
    }
    // Tesla exception
    if ((addr == 0x348) && (len == 8)) {
      // GTW_status
      ignition_can = (GET_BYTE(to_push, 0) & 0x1) != 0;
    }
  }
}


void fdcan_set_forwarding(int from, int to) {
  fdcan_forwarding[from] = to;
}


bool can_tx_check_min_slots_free(uint32_t min) {
  return
    (can_slots_empty(&can_tx1_q) >= min) &&
    (can_slots_empty(&can_tx2_q) >= min) &&
    (can_slots_empty(&can_tx3_q) >= min) &&
    (can_slots_empty(&can_txgmlan_q) >= min);
}

//REDEBUG: move to board header? Also should be cycled only one that is in question? 
// This MCU has no Bus_Off auto recovery so that's the replacement
void cycle_transceivers(void) {
  current_board->enable_can_transceiver(1, false);
  current_board->enable_can_transceiver(2, false);
  current_board->enable_can_transceiver(3, false);
  current_board->enable_can_transceiver(4, false);
  delay_ms(2);
  current_board->enable_can_transceiver(1, true);
  current_board->enable_can_transceiver(2, true);
  current_board->enable_can_transceiver(3, true);
  current_board->enable_can_transceiver(4, true);
}


void process_can(uint8_t can_number) {
  FDCAN_GlobalTypeDef *FDCANx = FDCANIF_FROM_FDCAN_NUM(can_number);

  if (can_number != 0xffU) {
    ENTER_CRITICAL();

    uint8_t bus_number = BUS_NUM_FROM_FDCAN_NUM(can_number);
    
    FDCANx->IR |= FDCAN_IR_TFE; // Clear Tx FIFO Empty flag
    
    // REDEBUG: need to rewrite it with TX events??
    //can_txd_cnt += 1;
    // Why to add successfully transmited message to my fifo??? Returning back with changed bus for Cabana?
    // if ((CAN->TSR & CAN_TSR_TXOK0) == CAN_TSR_TXOK0) {
    //   CAN_FIFOMailBox_TypeDef to_push;
    //   to_push.RIR = CAN->sTxMailBox[0].TIR;
    //   to_push.RDTR = (CAN->sTxMailBox[0].TDTR & 0xFFFF000FU) | ((CAN_BUS_RET_FLAG | bus_number) << 4);
    //   to_push.RDLR = CAN->sTxMailBox[0].TDLR;
    //   to_push.RDHR = CAN->sTxMailBox[0].TDHR;
    //   can_send_errs += can_push(&can_rx_q, &to_push) ? 0U : 1U;
    // }

    if ((FDCANx->TXFQS & FDCAN_TXFQS_TFQF) == 0) {
      CAN_FIFOMailBox_TypeDef to_send;
      if (can_pop(can_queues[bus_number], &to_send)) {
        can_tx_cnt += 1;
        uint32_t TxFIFOSA = FDCAN_START_ADDRESS + (can_number * FDCAN_OFFSET) + (FDCAN_RX_FIFO_0_EL_CNT * FDCAN_RX_FIFO_0_EL_SIZE);
        uint8_t tx_index = (FDCANx->TXFQS >> FDCAN_TXFQS_TFQPI_Pos) & 0x1F;
        // only send if we have received a packet
        CAN_FIFOMailBox_TypeDef *fifo;
        fifo = (CAN_FIFOMailBox_TypeDef *)(TxFIFOSA + tx_index * FDCAN_TX_FIFO_EL_SIZE);

        // Convert mailbox "type" to normal CAN
        fifo->RIR = ((to_send.RIR & 0x4) << 27) | ((to_send.RIR & 0x2) << 28) | (to_send.RIR >> 3);  // identifier format | frame type | identifier
        //REDEBUG: enable CAN FD and BRS for test purposes
        fifo->RDTR = ((to_send.RDTR & 0xF) << 16) | ((to_send.RDTR) >> 16) | (1U << 21) | (1U << 20); // DLC (length) | timestamp | enable CAN FD | enable BRS
        //fifo->RDTR = ((to_send.RDTR & 0xF) << 16) | ((to_send.RDTR) >> 16); // DLC (length) | timestamp
        fifo->RDLR = to_send.RDLR;
        fifo->RDHR = to_send.RDHR;
        
        FDCANx->TXBAR = (1UL << tx_index); 

        if (can_tx_check_min_slots_free(MAX_CAN_MSGS_PER_BULK_TRANSFER)) {
          usb_outep3_resume_if_paused();
        }
      }
    }
    EXIT_CRITICAL();
  }
  // Needed to fix periodical problem with transceiver sticking
  if ((FDCANx->PSR & FDCAN_PSR_BO) != 0 && (FDCANx->CCCR & FDCAN_CCCR_INIT) != 0) {
    puts("FDCAN is in Bus_Off state! Resetting... CAN number: "); puth(can_number); puts("\n");
    cycle_transceivers();
    FDCANx->IR = 0xFFC60000U; // Reset all flags(Only errors!)
    FDCANx->CCCR &= ~(FDCAN_CCCR_INIT);
    uint32_t timeout_counter = 0U;
    while((FDCANx->CCCR & FDCAN_CCCR_INIT) != 0) {
      // Delay for about 1ms
      delay_ms(1);
      timeout_counter++;

      if(timeout_counter >= FDCAN_INIT_TIMEOUT_MS){
        puts(FDCAN_NAME_FROM_FDCANIF(FDCANx)); puts(" Bus_Off reset timed out!\n");
        break;
      }
    }
  }
}


void can_send(CAN_FIFOMailBox_TypeDef *to_push, uint8_t bus_number, bool skip_tx_hook) {
  if (skip_tx_hook || safety_tx_hook(to_push) != 0) {
    if (bus_number < BUS_MAX) {
      // add CAN packet to send queue
      // bus number isn't passed through
      to_push->RDTR &= 0xF;
      if ((bus_number == 3U) && (fdcan_num_lookup[3] == 0xFFU)) {
        gmlan_send_errs += bitbang_gmlan(to_push) ? 0U : 1U;
      } else {
        //can_fwd_errs += can_push(can_queues[bus_number], to_push) ? 0U : 1U;
        //REDEBUG
        if (can_push(can_queues[bus_number], to_push) == false) {
          can_fwd_errs += 1;
          puts("TX can_send failed because of can_push!");
        }
        //REDEBUG
        process_can(FDCAN_NUM_FROM_BUS_NUM(bus_number));
      }
    }
  }
}


void can_rx(uint8_t can_number) {
  FDCAN_GlobalTypeDef *FDCANx = FDCANIF_FROM_FDCAN_NUM(can_number);
  uint8_t bus_number = BUS_NUM_FROM_FDCAN_NUM(can_number);
	uint8_t rx_fifo_idx;

	// Rx FIFO 0 new message
	if((FDCANx->IR & FDCAN_IR_RF0N) != 0) {
    FDCANx->IR |= FDCAN_IR_RF0N;
    while((FDCANx->RXF0S & FDCAN_RXF0S_F0FL) != 0) {
      can_rx_cnt += 1;

      // can is live
      pending_can_live = 1;

      // getting new message index (0 to 63)
      rx_fifo_idx = (uint8_t)((FDCANx->RXF0S >> FDCAN_RXF0S_F0GI_Pos) & 0x3F);

      uint32_t RxFIFO0SA = FDCAN_START_ADDRESS + (can_number * FDCAN_OFFSET);
      CAN_FIFOMailBox_TypeDef to_push;
      CAN_FIFOMailBox_TypeDef *fifo;

      // getting address
      fifo = (CAN_FIFOMailBox_TypeDef *)(RxFIFO0SA + rx_fifo_idx * FDCAN_RX_FIFO_0_EL_SIZE);

      // Need to convert real CAN frame format to mailbox "type"
      to_push.RIR = ((fifo->RIR >> 27) & 0x4) | ((fifo->RIR >> 28) & 0x2) | (fifo->RIR << 3); // identifier format | frame type | identifier
      to_push.RDTR = ((fifo->RDTR >> 16) & 0xF) | (fifo->RDTR << 16); // DLC (length) | timestamp
      to_push.RDLR = fifo->RDLR;
      to_push.RDHR = fifo->RDHR;

      // modify RDTR for our API
      to_push.RDTR = (to_push.RDTR & 0xFFFF000F) | (bus_number << 4);

      // forwarding (panda only)
      int bus_fwd_num = (fdcan_forwarding[bus_number] != -1) ? fdcan_forwarding[bus_number] : safety_fwd_hook(bus_number, &to_push);
      if (bus_fwd_num != -1) {
        CAN_FIFOMailBox_TypeDef to_send;
        to_send.RIR = to_push.RIR;
        to_send.RDTR = to_push.RDTR;
        to_send.RDLR = to_push.RDLR;
        to_send.RDHR = to_push.RDHR;
        can_send(&to_send, bus_fwd_num, true);
      }

      can_rx_errs += safety_rx_hook(&to_push) ? 0U : 1U;
      ignition_can_hook(&to_push);

      current_board->set_led(LED_BLUE, true);
      can_send_errs += can_push(&can_rx_q, &to_push) ? 0U : 1U;

      // update read index 
      FDCANx->RXF0A = rx_fifo_idx;
    }

  } else if((FDCANx->IR & (FDCAN_IR_PEA | FDCAN_IR_PED | FDCAN_IR_RF0L | FDCAN_IR_RF0F | FDCAN_IR_EW | FDCAN_IR_MRAF | FDCAN_IR_TOO)) != 0) {
    #ifdef DEBUG
      puts("FDCAN error, FDCAN_IR: ");puth(FDCANx->IR);puts("\n");
    #endif
    FDCANx->IR |= (FDCAN_IR_PEA | FDCAN_IR_PED | FDCAN_IR_RF0L | FDCAN_IR_RF0F | FDCAN_IR_EW | FDCAN_IR_MRAF | FDCAN_IR_TOO); // Clean all error flags
    can_err_cnt += 1;
  }
  
}




void FDCAN1_IT0_IRQ_Handler(void) { can_rx(0); }
void FDCAN1_IT1_IRQ_Handler(void) { process_can(0); }
void FDCAN2_IT0_IRQ_Handler(void) { can_rx(1); }
void FDCAN2_IT1_IRQ_Handler(void) { process_can(1); }
void FDCAN3_IT0_IRQ_Handler(void) { can_rx(2);  }
void FDCAN3_IT1_IRQ_Handler(void) { process_can(2); }




bool fdcan_set_speed(uint8_t can_number) {
  bool ret = true;
  FDCAN_GlobalTypeDef *FDCANx = FDCANIF_FROM_FDCAN_NUM(can_number);
  uint8_t bus_number = BUS_NUM_FROM_FDCAN_NUM(can_number);

  ret &= llfdcan_set_speed(FDCANx, can_speed[bus_number], can_loopback, (unsigned int)(can_silent) & (1U << can_number));
  return ret;
}


bool fdcan_init(uint8_t can_number) {
  bool ret = false;

  REGISTER_INTERRUPT(FDCAN1_IT0_IRQn, FDCAN1_IT0_IRQ_Handler, CAN_INTERRUPT_RATE, FAULT_INTERRUPT_RATE_CAN_1)
  REGISTER_INTERRUPT(FDCAN1_IT1_IRQn, FDCAN1_IT1_IRQ_Handler, CAN_INTERRUPT_RATE, FAULT_INTERRUPT_RATE_CAN_1)
  REGISTER_INTERRUPT(FDCAN2_IT0_IRQn, FDCAN2_IT0_IRQ_Handler, CAN_INTERRUPT_RATE, FAULT_INTERRUPT_RATE_CAN_2)
  REGISTER_INTERRUPT(FDCAN2_IT1_IRQn, FDCAN2_IT1_IRQ_Handler, CAN_INTERRUPT_RATE, FAULT_INTERRUPT_RATE_CAN_2)
  REGISTER_INTERRUPT(FDCAN3_IT0_IRQn, FDCAN3_IT0_IRQ_Handler, CAN_INTERRUPT_RATE, FAULT_INTERRUPT_RATE_CAN_3)
  REGISTER_INTERRUPT(FDCAN3_IT1_IRQn, FDCAN3_IT1_IRQ_Handler, CAN_INTERRUPT_RATE, FAULT_INTERRUPT_RATE_CAN_3)

  if (can_number != 0xffU) {
    FDCAN_GlobalTypeDef *FDCANx = FDCANIF_FROM_FDCAN_NUM(can_number);
    ret &= fdcan_set_speed(can_number);
    ret &= llfdcan_init(FDCANx);
    // in case there are queued up messages
    process_can(can_number);
  }
  return ret;
}



void fdcan_init_all(void) {
  bool ret = true;
  for (uint8_t i=0U; i < FDCAN_MAX; i++) {
    can_clear(can_queues[i]);
    ret &= fdcan_init(i);
  }
  UNUSED(ret);
}


void fdcan_set_obd(uint8_t harness_orientation, bool obd){
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
      // B12,B13: disable normal mode
        set_gpio_pullup(GPIOB, 12, PULL_NONE);
        set_gpio_mode(GPIOB, 12, MODE_ANALOG);

        set_gpio_pullup(GPIOB, 13, PULL_NONE);
        set_gpio_mode(GPIOB, 13, MODE_ANALOG);
        // B5,B6: FDCAN2 mode
        set_gpio_output_type(GPIOB, 5, OUTPUT_TYPE_PUSH_PULL);
        set_gpio_pullup(GPIOB, 5, PULL_NONE);
        set_gpio_speed(GPIOB, 5, SPEED_LOW);
        set_gpio_alternate(GPIOB, 5, GPIO_AF9_FDCAN2);

        set_gpio_output_type(GPIOB, 6, OUTPUT_TYPE_PUSH_PULL);
        set_gpio_pullup(GPIOB, 6, PULL_NONE);
        set_gpio_speed(GPIOB, 6, SPEED_LOW);
        set_gpio_alternate(GPIOB, 6, GPIO_AF9_FDCAN2);
    }
  } else {
    puts("OBD CAN not available on this board\n");
  }
}

#define CAN_NUM_FROM_BUS_NUM(x) (FDCAN_NUM_FROM_BUS_NUM(x))
void can_init_all(void) { fdcan_init_all(); }
void can_set_forwarding(int from, int to) { fdcan_set_forwarding(from, to); }
bool can_init(uint8_t can_number) { return fdcan_init(can_number); }
void can_flip_buses(uint8_t bus1, uint8_t bus2){ fdcan_flip_buses(bus1, bus2); }
void can_set_obd(uint8_t harness_orientation, bool obd){ fdcan_set_obd(harness_orientation, obd); }
