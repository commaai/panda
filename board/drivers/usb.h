#ifndef USB_H
#define USB_H

#include "board/drivers/drivers.h"

typedef union {
  uint16_t w;
  struct BW {
    uint8_t msb;
    uint8_t lsb;
  } bw;
} uint16_t_uint8_t;

// cppcheck-suppress misra-c2012-2.3 ; used in driver implementations
typedef union _USB_Setup {
  uint32_t d8[2];
  struct _SetupPkt_Struc {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t_uint8_t wValue;
    uint16_t_uint8_t wIndex;
    uint16_t_uint8_t wLength;
  } b;
} USB_Setup_TypeDef;

#define  USB_REQ_GET_STATUS                             0x00
#define  USB_REQ_SET_ADDRESS                            0x05
#define  USB_REQ_GET_DESCRIPTOR                         0x06
#define  USB_REQ_SET_CONFIGURATION                      0x09
#define  USB_REQ_SET_INTERFACE                          0x0B
#define  USB_DESC_TYPE_DEVICE                           0x01
#define  USB_DESC_TYPE_CONFIGURATION                    0x02
#define  USB_DESC_TYPE_STRING                           0x03
#define  USB_DESC_TYPE_INTERFACE                        0x04
#define  USB_DESC_TYPE_ENDPOINT                         0x05
#define  USB_DESC_TYPE_DEVICE_QUALIFIER                 0x06
#define  USB_DESC_TYPE_BINARY_OBJECT_STORE              0x0f
#define  STRING_OFFSET_LANGID                           0x00
#define  STRING_OFFSET_IMANUFACTURER                    0x01
#define  STRING_OFFSET_IPRODUCT                         0x02
#define  STRING_OFFSET_ISERIAL                          0x03
#define  STRING_OFFSET_ICONFIGURATION                   0x04
#define  WINUSB_REQ_GET_COMPATID_DESCRIPTOR             0x04
#define  WINUSB_REQ_GET_EXT_PROPS_OS                    0x05
#define  WINUSB_REQ_GET_DESCRIPTOR                      0x07
#define  STS_DATA_UPDT                          2
#define  STS_SETUP_UPDT                         6
#define  DSCR_INTERFACE_LEN 9
#define  DSCR_ENDPOINT_LEN 7
#define  DSCR_CONFIG_LEN 9
#define  DSCR_DEVICE_LEN 18
#define  ENDPOINT_TYPE_BULK 2
#define  ENDPOINT_TYPE_INT 3
#define  MS_VENDOR_CODE 0x20
#define  WEBUSB_VENDOR_CODE 0x30
#define  BINARY_OBJECT_STORE_DESCRIPTOR_LENGTH   0x05
#define  BINARY_OBJECT_STORE_DESCRIPTOR          0x0F
#define  WINUSB_PLATFORM_DESCRIPTOR_LENGTH       0x9E
#define  TOUSBORDER(num) ((num) & 0xFFU), (((uint16_t)(num) >> 8) & 0xFFU)
#define  STRING_DESCRIPTOR_HEADER(size) (((((size) * 2) + 2) & 0xFF) | 0x0300)
#define  ENDPOINT_RCV 0x80
#define  ENDPOINT_SND 0x00

extern bool outep3_processing;

void *USB_ReadPacket(void *dest, uint16_t len);
void USB_WritePacket(const void *src, uint16_t len, uint32_t ep);
void USB_WritePacket_EP0(uint8_t *src, uint16_t len);
void usb_reset(void);
void usb_setup(void);
void usb_irqhandler(void);
void can_tx_comms_resume_usb(void);

#endif
