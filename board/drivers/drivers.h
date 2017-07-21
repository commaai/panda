// ********************* USB *********************
// IRQs: OTG_FS

typedef union {
  uint16_t w;
  struct BW {
    uint8_t msb;
    uint8_t lsb;
  }
  bw;
}
uint16_t_uint8_t;

typedef union _USB_Setup {
  uint32_t d8[2];
  struct _SetupPkt_Struc
  {
    uint8_t           bmRequestType;
    uint8_t           bRequest;
    uint16_t_uint8_t  wValue;
    uint16_t_uint8_t  wIndex;
    uint16_t_uint8_t  wLength;
  } b;
}
USB_Setup_TypeDef;

void usb_init();
int usb_cb_control_msg(USB_Setup_TypeDef *setup, uint8_t *resp, int hardwired);
int usb_cb_ep1_in(uint8_t *usbdata, int len, int hardwired);
void usb_cb_ep2_out(uint8_t *usbdata, int len, int hardwired);
void usb_cb_ep3_out(uint8_t *usbdata, int len, int hardwired);
void usb_cb_enumeration_complete();


// ********************* UART *********************
// IRQs: USART1, USART2, USART3, UART5

#define FIFO_SIZE 0x100
typedef struct uart_ring {
  uint8_t w_ptr_tx;
  uint8_t r_ptr_tx;
  uint8_t elems_tx[FIFO_SIZE];
  uint8_t w_ptr_rx;
  uint8_t r_ptr_rx;
  uint8_t elems_rx[FIFO_SIZE];
  USART_TypeDef *uart;
  void (*callback)(struct uart_ring*);
} uart_ring;

int getc(uart_ring *q, char *elem);
int putc(uart_ring *q, char elem);

int puts(const char *a);
void puth(unsigned int i);
void hexdump(void *a, int l);


// ********************* ADC *********************

void adc_init();
uint32_t adc_get(int channel);


// ********************* TIMER *********************

void timer_init(TIM_TypeDef *TIM, int psc);


// ********************* SPI *********************
// IRQs: DMA2_Stream2, DMA2_Stream3, EXTI4

void spi_init();
void spi_cb_rx(uint8_t *data, int len);
void spi_tx_dma(void *addr, int len);


// ********************* CAN *********************
// IRQs: CAN1_TX, CAN1_RX0, CAN1_SCE
//       CAN2_TX, CAN2_RX0, CAN2_SCE
//       CAN3_TX, CAN3_RX0, CAN3_SCE

typedef struct {
  uint32_t w_ptr;
  uint32_t r_ptr;
  uint32_t fifo_size;
  CAN_FIFOMailBox_TypeDef *elems;
} can_ring;

#define CAN_BUS_RET_FLAG 0x80
#define CAN_BUS_NUM_MASK 0x7F

#ifdef PANDA
  #define BUS_MAX 4
#else
  #define BUS_MAX 2
#endif

extern int can_live, pending_can_live;

// must reinit after changing these
extern int can_loopback, can_silent;
extern uint32_t can_speed[];

void can_set_forwarding(int from, int to);

void can_init(uint8_t can_number);
void can_init_all();
void can_send(CAN_FIFOMailBox_TypeDef *to_push, uint8_t bus_number);
int can_pop(can_ring *q, CAN_FIFOMailBox_TypeDef *elem);


