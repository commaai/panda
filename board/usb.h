#ifndef PANDA_USB_H
#define PANDA_USB_H

// **** supporting defines ****

typedef struct
{
  __IO uint32_t HPRT;
}
USB_OTG_HostPortTypeDef;

#define USBx USB_OTG_FS
#define USBx_HOST       ((USB_OTG_HostTypeDef *)((uint32_t)USBx + USB_OTG_HOST_BASE))
#define USBx_HOST_PORT  ((USB_OTG_HostPortTypeDef *)((uint32_t)USBx + USB_OTG_HOST_PORT_BASE))
#define USBx_DEVICE     ((USB_OTG_DeviceTypeDef *)((uint32_t)USBx + USB_OTG_DEVICE_BASE))
#define USBx_INEP(i)    ((USB_OTG_INEndpointTypeDef *)((uint32_t)USBx + USB_OTG_IN_ENDPOINT_BASE + (i)*USB_OTG_EP_REG_SIZE))
#define USBx_OUTEP(i)   ((USB_OTG_OUTEndpointTypeDef *)((uint32_t)USBx + USB_OTG_OUT_ENDPOINT_BASE + (i)*USB_OTG_EP_REG_SIZE))
#define USBx_DFIFO(i)   *(__IO uint32_t *)((uint32_t)USBx + USB_OTG_FIFO_BASE + (i) * USB_OTG_FIFO_SIZE)
#define USBx_PCGCCTL    *(__IO uint32_t *)((uint32_t)USBx + USB_OTG_PCGCCTL_BASE)

#define  USB_REQ_GET_STATUS                             0x00
#define  USB_REQ_CLEAR_FEATURE                          0x01
#define  USB_REQ_SET_FEATURE                            0x03
#define  USB_REQ_SET_ADDRESS                            0x05
#define  USB_REQ_GET_DESCRIPTOR                         0x06
#define  USB_REQ_SET_DESCRIPTOR                         0x07
#define  USB_REQ_GET_CONFIGURATION                      0x08
#define  USB_REQ_SET_CONFIGURATION                      0x09
#define  USB_REQ_GET_INTERFACE                          0x0A
#define  USB_REQ_SET_INTERFACE                          0x0B
#define  USB_REQ_SYNCH_FRAME                            0x0C

#define  USB_DESC_TYPE_DEVICE                              1
#define  USB_DESC_TYPE_CONFIGURATION                       2
#define  USB_DESC_TYPE_STRING                              3
#define  USB_DESC_TYPE_INTERFACE                           4
#define  USB_DESC_TYPE_ENDPOINT                            5
#define  USB_DESC_TYPE_DEVICE_QUALIFIER                    6
#define  USB_DESC_TYPE_OTHER_SPEED_CONFIGURATION           7

#define STS_GOUT_NAK                           1
#define STS_DATA_UPDT                          2
#define STS_XFER_COMP                          3
#define STS_SETUP_COMP                         4
#define STS_SETUP_UPDT                         6

#define USBD_FS_TRDT_VALUE           5

#define USB_OTG_SPEED_FULL 3

#define MAX_RESP_LEN 0x40
uint8_t resp[MAX_RESP_LEN];

typedef union
{
  uint16_t w;
  struct BW
  {
    uint8_t msb;
    uint8_t lsb;
  }
  bw;
}
uint16_t_uint8_t;


typedef union _USB_Setup
{
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

// interfaces
int  usb_cb_control_msg(USB_Setup_TypeDef *setup, uint8_t *usbdata, int hardwired);
void usb_cb_ep0_out(USB_Setup_TypeDef *setup, uint8_t *usbdata, int hardwired);
int  usb_cb_ep1_in(uint8_t *usbdata, int len, int hardwired);
void usb_cb_ep2_out(uint8_t *usbdata, int len, int hardwired);
void usb_cb_ep3_out(uint8_t *usbdata, int len, int hardwired);

extern int did_usb_enumerate;

// descriptor types
// same as setupdat.h
#define DSCR_DEVICE_TYPE 1
#define DSCR_CONFIG_TYPE 2
#define DSCR_STRING_TYPE 3
#define DSCR_INTERFACE_TYPE 4
#define DSCR_ENDPOINT_TYPE 5
#define DSCR_DEVQUAL_TYPE 6

// for the repeating interfaces
#define DSCR_INTERFACE_LEN 9
#define DSCR_ENDPOINT_LEN 7
#define DSCR_CONFIG_LEN 9
#define DSCR_DEVICE_LEN 18

// endpoint types
#define ENDPOINT_TYPE_CONTROL 0
#define ENDPOINT_TYPE_ISO 1
#define ENDPOINT_TYPE_BULK 2
#define ENDPOINT_TYPE_INT 3

//Convert machine byte order to USB byte order
#define TOUSBORDER(num)\
  (num&0xFF), ((num>>8)&0xFF)

#define ENDPOINT_RCV 0x80
#define ENDPOINT_SND 0x00

// current packet
extern USB_Setup_TypeDef setup;
extern uint8_t usbdata[0x100];

// Store the current interface alt setting.
extern int current_int0_alt_setting;

// packet read and write

void *USB_ReadPacket(void *dest, uint16_t len);

void USB_WritePacket(const uint8_t *src, uint16_t len, uint32_t ep);

void USB_Stall_EP0();

void usb_reset();

char to_hex_char(int a);

void usb_setup();

void usb_init();

// ***************************** USB port *****************************
void usb_irqhandler(void);

#endif
