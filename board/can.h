// assign CAN numbering
// bus num: Can bus number on ODB connector. Sent to/from USB
//    Min: 0; Max: 127; Bit 7 marks message as receipt (bus 129 is receipt for but 1)
// cans: Look up MCU can interface from bus number
// can number: numeric lookup for MCU CAN interfaces (0 = CAN1, 1 = CAN2, etc);
// bus_lookup: Translates from 'can number' to 'bus number'.
// can_num_lookup: Translates from 'bus number' to 'can number'.
// can_forwarding: Given a bus num, lookup bus num to forward to. -1 means no forward.


// NEO:         Bus 1=CAN1   Bus 2=CAN2
// Panda:       Bus 0=CAN1   Bus 1=CAN2   Bus 2=CAN3
#ifdef PANDA
  CAN_TypeDef *cans[] = {CAN1, CAN2, CAN3};
  uint8_t bus_lookup[] = {0,1,2};
  uint8_t can_num_lookup[] = {0,1,2}; //bus num -> can num
  int8_t can_forwarding[] = {-1,-1,-1};
  uint32_t can_speed[] = {5000, 5000, 5000}; // 500 kbps
  #define CAN_MAX 3
#else
  CAN_TypeDef *cans[] = {CAN2, CAN1};
  uint8_t bus_lookup[] = {1,0};
  uint8_t can_num_lookup[] = {1,0}; //bus num -> can num
  int8_t can_forwarding[] = {-1,-1};
  uint32_t can_speed[] = {5000, 5000};
  #define CAN_MAX 2
#endif


#define NO_ACTIVE_GMLAN -1
int active_gmlan_port_id = NO_ACTIVE_GMLAN;

#define CANIF_FROM_CAN_NUM(num) (cans[bus_lookup[num]])
#define CANIF_FROM_BUS_NUM(num) (cans[num])
#define BUS_NUM_FROM_CAN_NUM(num) (bus_lookup[num])
#define CAN_NUM_FROM_BUS_NUM(num) (can_num_lookup[num])

#define CAN_BUS_RET_FLAG 0x80
#define CAN_BUS_NUM_MASK 0x7F

/*#define CAN_QUANTA 16
#define CAN_SEQ1 13
#define CAN_SEQ2 2*/

// this is needed for 1 mbps support
#define CAN_QUANTA 8
#define CAN_SEQ1 6 // roundf(quanta * 0.875f) - 1;
#define CAN_SEQ2 1 // roundf(quanta * 0.125f);

#define CAN_PCLK 24000
// 333 = 33.3 kbps
// 5000 = 500 kbps
#define can_speed_to_prescaler(x) (CAN_PCLK / CAN_QUANTA * 10 / (x))

void can_init(uint8_t bus_number) {
  CAN_TypeDef *CAN = CANIF_FROM_BUS_NUM(bus_number);
  set_can_enable(CAN, 1);

  CAN->MCR = CAN_MCR_TTCM | CAN_MCR_INRQ;
  while((CAN->MSR & CAN_MSR_INAK) != CAN_MSR_INAK);

  // set time quanta from defines
  CAN->BTR = (CAN_BTR_TS1_0 * (CAN_SEQ1-1)) |
             (CAN_BTR_TS2_0 * (CAN_SEQ2-1)) |
             (can_speed_to_prescaler(can_speed[bus_number]) - 1);

  // silent loopback mode for debugging
  if (can_loopback) {
    CAN->BTR |= CAN_BTR_SILM | CAN_BTR_LBKM;
  }

  if (can_silent) {
    CAN->BTR |= CAN_BTR_SILM;
  }

  // reset
  CAN->MCR = CAN_MCR_TTCM;

  #define CAN_TIMEOUT 1000000
  int tmp = 0;
  while((CAN->MSR & CAN_MSR_INAK) == CAN_MSR_INAK && tmp < CAN_TIMEOUT) tmp++;

  if (tmp == CAN_TIMEOUT) {
    set_led(LED_BLUE, 1);
    puts("CAN init FAILED!!!!!\n");
  } else {
    puts("CAN init done\n");
  }

  // accept all filter
  CAN->FMR |= CAN_FMR_FINIT;

  // no mask
  CAN->sFilterRegister[0].FR1 = 0;
  CAN->sFilterRegister[0].FR2 = 0;
  CAN->sFilterRegister[14].FR1 = 0;
  CAN->sFilterRegister[14].FR2 = 0;
  CAN->FA1R |= 1 | (1 << 14);

  CAN->FMR &= ~(CAN_FMR_FINIT);

  // enable all CAN interrupts
  //CAN->IER = 0xFFFFFFFF;
  //CAN->IER = CAN_IER_TMEIE | CAN_IER_FMPIE0 | CAN_IER_FMPIE1;
  //CAN->IER = CAN_IER_TMEIE;
  CAN->IER = CAN_IER_TMEIE | CAN_IER_FMPIE0;
}

void can_init_all() {
  for (int i=0; i < CAN_MAX; i++) {
    can_init(i);
  }
}

// CAN error
void can_sce(CAN_TypeDef *CAN) {
  #ifdef DEBUG
    if (CAN==CAN1) puts("CAN1:  ");
    if (CAN==CAN2) puts("CAN2:  ");
    #ifdef CAN3
      if (CAN==CAN3) puts("CAN3:  ");
    #endif
    puts("MSR:");
    puth(CAN->MSR);
    puts(" TSR:");
    puth(CAN->TSR);
    puts(" RF0R:");
    puth(CAN->RF0R);
    puts(" RF1R:");
    puth(CAN->RF1R);
    puts(" ESR:");
    puth(CAN->ESR);
    puts("\n");
  #endif

  // clear
  //CAN->sTxMailBox[0].TIR &= ~(CAN_TI0R_TXRQ);
  CAN->TSR |= CAN_TSR_ABRQ0;
  //CAN->ESR |= CAN_ESR_LEC;
  //CAN->MSR &= ~(CAN_MSR_ERRI);
  CAN->MSR = CAN->MSR;
}

int can_cksum(uint8_t *dat, int len, int addr, int idx) {
  int i;
  int s = 0;
  for (i = 0; i < len; i++) {
    s += (dat[i] >> 4);
    s += dat[i] & 0xF;
  }
  s += (addr>>0)&0xF;
  s += (addr>>4)&0xF;
  s += (addr>>8)&0xF;
  s += idx;
  s = 8-s;
  return s&0xF;
}

