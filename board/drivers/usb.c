#include "board/config.h"
#include "board/drivers/usb.h"
#include "board/drivers/uart.h"
#include "board/can_comms.h"
#include "board/flasher.h"
#include "board/sys/critical.h"
#include "board/utils.h"
#include "board/libc.h"

uint8_t response[USBPACKET_MAX_SIZE];
USB_Setup_TypeDef setup;
uint8_t* ep0_txdata = NULL;
static uint16_t ep0_txlen = 0;
bool outep3_processing = false;
static int current_int0_alt_setting = 0;

void *USB_ReadPacket(void *dest, uint16_t len) {
  uint32_t *dest_copy = (uint32_t *)dest;
  uint32_t count32b = ((uint32_t)len + 3U) / 4U;

  for (uint32_t i = 0; i < count32b; i++) {
    *dest_copy = USBx_DFIFO(0U);
    dest_copy++;
  }
  return ((void *)dest_copy);
}

void USB_WritePacket(const void *src, uint16_t len, uint32_t ep) {
  #ifdef DEBUG_USB
  print("writing ");
  hexdump(src, len);
  #endif

  uint32_t numpacket = ((uint32_t)len + (USBPACKET_MAX_SIZE - 1U)) / USBPACKET_MAX_SIZE;
  uint32_t count32b = 0;
  count32b = ((uint32_t)len + 3U) / 4U;

  USBx_INEP(ep)->DIEPTSIZ = ((numpacket << 19) & USB_OTG_DIEPTSIZ_PKTCNT) |
                            (len               & USB_OTG_DIEPTSIZ_XFRSIZ);
  USBx_INEP(ep)->DIEPCTL |= (USB_OTG_DIEPCTL_CNAK | USB_OTG_DIEPCTL_EPENA);

  if (src != NULL) {
    const uint32_t *src_copy = (const uint32_t *)src;
    for (uint32_t i = 0; i < count32b; i++) {
      USBx_DFIFO(ep) = *src_copy;
      src_copy++;
    }
  }
}

void USB_WritePacket_EP0(uint8_t *src, uint16_t len) {
  #ifdef DEBUG_USB
  print("writing ");
  hexdump(src, len);
  #endif

  uint16_t wplen = MIN(len, 0x40);
  USB_WritePacket(src, wplen, 0);

  if (wplen < len) {
    ep0_txdata = &src[wplen];
    ep0_txlen = len - wplen;
    USBx_DEVICE->DIEPEMPMSK |= 1;
  } else {
    USBx_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK;
  }
}

void usb_reset(void) {
  USBx_DEVICE->DAINT = 0xFFFFFFFFU;
  USBx_DEVICE->DAINTMSK = 0xFFFFFFFFU;
  USBx_DEVICE->DIEPMSK = 0xFFFFFFFFU;
  USBx_DEVICE->DOEPMSK = 0xFFFFFFFFU;

  USBx_INEP(0U)->DIEPINT = 0xFF;
  USBx_OUTEP(0U)->DOEPINT = 0xFF;

  USBx_DEVICE->DCFG &= ~USB_OTG_DCFG_DAD;

  USBx->GRXFSIZ = 0x40;
  USBx->DIEPTXF0_HNPTXFSIZ = (0x40UL << 16) | 0x40U;
  USBx->DIEPTXF[0] = (0x40UL << 16) | 0x80U;

  USBx->GRSTCTL = USB_OTG_GRSTCTL_TXFFLSH | USB_OTG_GRSTCTL_TXFNUM_4;
  while ((USBx->GRSTCTL & USB_OTG_GRSTCTL_TXFFLSH) == USB_OTG_GRSTCTL_TXFFLSH);
  USBx->GRSTCTL = USB_OTG_GRSTCTL_RXFFLSH;
  while ((USBx->GRSTCTL & USB_OTG_GRSTCTL_RXFFLSH) == USB_OTG_GRSTCTL_RXFFLSH);

  USBx_DEVICE->DCTL |= USB_OTG_DCTL_CGINAK;
  USBx_OUTEP(0U)->DOEPTSIZ = USB_OTG_DOEPTSIZ_STUPCNT | (USB_OTG_DOEPTSIZ_PKTCNT & (1UL << 19)) | (3U << 3);
}

char to_hex_char(uint8_t a) {
  char ret;
  if (a < 10U) {
    ret = '0' + a;
  } else {
    ret = 'a' + (a - 10U);
  }
  return ret;
}

void usb_setup(void) {
  static uint8_t device_desc[] = {
    DSCR_DEVICE_LEN, USB_DESC_TYPE_DEVICE, //Length, Type
    0x10, 0x02, // bcdUSB max version of USB supported (2.1)
    0xFF, 0xFF, 0xFF, 0x40, // Class, Subclass, Protocol, Max Packet Size
    TOUSBORDER(USB_VID), // idVendor
    TOUSBORDER(USB_PID), // idProduct
    0x00, 0x00, // bcdDevice
    0x01, 0x02, // Manufacturer, Product
    0x03, 0x01 // Serial Number, Num Configurations
  };

  static uint8_t device_qualifier[] = {
    0x0a, USB_DESC_TYPE_DEVICE_QUALIFIER, //Length, Type
    0x10, 0x02, // bcdUSB max version of USB supported (2.1)
    0xFF, 0xFF, 0xFF, 0x40, // bDeviceClass, bDeviceSubClass, bDeviceProtocol, bMaxPacketSize0
    0x01, 0x00 // bNumConfigurations, bReserved
  };

  static uint8_t configuration_desc[] = {
    DSCR_CONFIG_LEN, USB_DESC_TYPE_CONFIGURATION, // Length, Type,
    TOUSBORDER(0x0045U), // Total Len (uint16)
    0x01, 0x01, STRING_OFFSET_ICONFIGURATION, // Num Interface, Config Value, Configuration
    0xc0, 0x32, // Attributes, Max Power
    DSCR_INTERFACE_LEN, USB_DESC_TYPE_INTERFACE, // Length, Type
    0x00, 0x00, 0x03, // Index, Alt Index idx, Endpoint count
    0XFF, 0xFF, 0xFF, // Class, Subclass, Protocol
    0x00, // Interface
      DSCR_ENDPOINT_LEN, USB_DESC_TYPE_ENDPOINT, // Length, Type
      ENDPOINT_RCV | 1, ENDPOINT_TYPE_BULK, // Endpoint Num/Direction, Type
      TOUSBORDER(0x0040U), // Max Packet (0x0040)
      0x00, // Polling Interval (NA)
      DSCR_ENDPOINT_LEN, USB_DESC_TYPE_ENDPOINT, // Length, Type
      ENDPOINT_SND | 2, ENDPOINT_TYPE_BULK, // Endpoint Num/Direction, Type
      TOUSBORDER(0x0040U), // Max Packet (0x0040)
      0x00, // Polling Interval
      DSCR_ENDPOINT_LEN, USB_DESC_TYPE_ENDPOINT, // Length, Type
      ENDPOINT_SND | 3, ENDPOINT_TYPE_BULK, // Endpoint Num/Direction, Type
      TOUSBORDER(0x0040U), // Max Packet (0x0040)
      0x00, // Polling Interval
    DSCR_INTERFACE_LEN, USB_DESC_TYPE_INTERFACE, // Length, Type
    0x00, 0x01, 0x03, // Index, Alt Index idx, Endpoint count
    0XFF, 0xFF, 0xFF, // Class, Subclass, Protocol
    0x00, // Interface
      DSCR_ENDPOINT_LEN, USB_DESC_TYPE_ENDPOINT, // Length, Type
      ENDPOINT_RCV | 1, ENDPOINT_TYPE_INT, // Endpoint Num/Direction, Type
      TOUSBORDER(0x0040U), // Max Packet (0x0040)
      0x05, // Polling Interval (5 frames)
      DSCR_ENDPOINT_LEN, USB_DESC_TYPE_ENDPOINT, // Length, Type
      ENDPOINT_SND | 2, ENDPOINT_TYPE_BULK, // Endpoint Num/Direction, Type
      TOUSBORDER(0x0040U), // Max Packet (0x0040)
      0x00, // Polling Interval
      DSCR_ENDPOINT_LEN, USB_DESC_TYPE_ENDPOINT, // Length, Type
      ENDPOINT_SND | 3, ENDPOINT_TYPE_BULK, // Endpoint Num/Direction, Type
      TOUSBORDER(0x0040U), // Max Packet (0x0040)
      0x00, // Polling Interval
  };

  static uint16_t string_language_desc[] = { STRING_DESCRIPTOR_HEADER(1), 0x0409 };
  static uint16_t string_manufacturer_desc[] = { STRING_DESCRIPTOR_HEADER(8), 'c', 'o', 'm', 'm', 'a', '.', 'a', 'i' };
  static uint16_t string_product_desc[] = { STRING_DESCRIPTOR_HEADER(5), 'p', 'a', 'n', 'd', 'a' };
  static uint16_t string_configuration_desc[] = { STRING_DESCRIPTOR_HEADER(2), '0', '1' };
  static uint8_t string_238_desc[] = { 0x12, USB_DESC_TYPE_STRING, 'M',0, 'S',0, 'F',0, 'T',0, '1',0, '0',0, '0',0, MS_VENDOR_CODE, 0x00 };
  static uint8_t winusb_ext_compatid_os_desc[] = { 0x28, 0x00, 0x00, 0x00, 0x00, 0x01, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  static uint8_t winusb_ext_prop_os_desc[] = { 0x8e, 0x00, 0x00, 0x00, 0x00, 0x01, 0x05, 0x00, 0x01, 0x00, 0x84, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x28, 0x00, 'D',0, 'e',0, 'v',0, 'i',0, 'c',0, 'e',0, 'I',0, 'n',0, 't',0, 'e',0, 'r',0, 'f',0, 'a',0, 'c',0, 'e',0, 'G',0, 'U',0, 'I',0, 'D',0, 0, 0, 0x4e, 0x00, 0x00, 0x00, '{',0, 'c',0, 'c',0, 'e',0, '5',0, '2',0, '9',0, '1',0, 'c',0, '-',0, 'a',0, '6',0, '9',0, 'f',0, '-',0, '4',0 ,'9',0 ,'9',0 ,'5',0 ,'-',0, 'a',0, '4',0, 'c',0, '2',0, '-',0, '2',0, 'a',0, 'e',0, '5',0, '7',0, 'a',0, '5',0, '1',0, 'a',0, 'd',0, 'e',0, '9',0, '}',0, 0, 0 };
  static uint8_t binary_object_store_desc[] = {
    BINARY_OBJECT_STORE_DESCRIPTOR_LENGTH, BINARY_OBJECT_STORE_DESCRIPTOR, 0x39, 0x00, 0x02,
    0x18, 0x10, 0x05, 0x00, 0x38, 0xB6, 0x08, 0x34, 0xA9, 0x09, 0xA0, 0x47, 0x8B, 0xFD, 0xA0, 0x76, 0x88, 0x15, 0xB6, 0x65, 0x00, 0x01, WEBUSB_VENDOR_CODE, 0x03,
    0x1C, 0x10, 0x05, 0x00, 0xDF, 0x60, 0xDD, 0xD8, 0x89, 0x45, 0xC7, 0x4C, 0x9C, 0xD2, 0x65, 0x9D, 0x9E, 0x64, 0x8A, 0x9F, 0x00, 0x00, 0x03, 0x06, WINUSB_PLATFORM_DESCRIPTOR_LENGTH, 0x00, MS_VENDOR_CODE, 0x00
  };
  static uint8_t winusb_20_desc[WINUSB_PLATFORM_DESCRIPTOR_LENGTH] = {
    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x06, WINUSB_PLATFORM_DESCRIPTOR_LENGTH, 0x00,
    0x14, 0x00, 0x03, 0x00, 'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x04, 0x00, 0x01, 0x00, 0x28, 0x00, 'D', 0x00, 'e', 0x00, 'v', 0x00, 'i', 0x00, 'c', 0x00, 'e', 0x00, 'I', 0x00, 'n', 0x00, 't', 0x00, 'e', 0x00, 'r', 0x00, 'f', 0x00, 'a', 0x00, 'c', 0x00, 'e', 0x00, 'G', 0x00, 'U', 0x00, 'I', 0x00, 'D', 0x00, 0x00, 0x00,
    0x4E, 0x00, '{', 0x00, 'c', 0x00, 'c', 0x00, 'e', 0x00, '5', 0x00, '2', 0x00, '9', 0x00, '1', 0x00, 'c', 0x00, '-', 0x00, 'a', 0x00, '6', 0x00, '9', 0x00, 'f', 0x00, '-', 0x00, '4', 0x00, '9', 0x00, '9', 0x00, '5', 0x00, '-', 0x00, 'a', 0x00, '4', 0x00, 'c', 0x00, '2', 0x00, '-', 0x00, '2', 0x00, 'a', 0x00, 'e', 0x00, '5', 0x00, '7', 0x00, 'a', 0x00, '5', 0x00, '1', 0x00, 'a', 0x00, 'd', 0x00, 'e', 0x00, '9', 0x00, '}', 0x00, 0x00, 0x00
  };

  int resp_len;
  ControlPacket_t control_req;

  switch (setup.b.bRequest) {
    case USB_REQ_SET_CONFIGURATION:
      USBx_INEP(1U)->DIEPCTL = (0x40U & USB_OTG_DIEPCTL_MPSIZ) | (2UL << 18) | (1UL << 22) | USB_OTG_DIEPCTL_SD0PID_SEVNFRM | USB_OTG_DIEPCTL_USBAEP;
      USBx_INEP(1U)->DIEPINT = 0xFF;
      USBx_OUTEP(2U)->DOEPTSIZ = (1UL << 19) | 0x40U;
      USBx_OUTEP(2U)->DOEPCTL = (0x40U & USB_OTG_DOEPCTL_MPSIZ) | (2UL << 18) | USB_OTG_DOEPCTL_SD0PID_SEVNFRM | USB_OTG_DOEPCTL_USBAEP;
      USBx_OUTEP(2U)->DOEPINT = 0xFF;
      USBx_OUTEP(3U)->DOEPTSIZ = (32UL << 19) | 0x800U;
      USBx_OUTEP(3U)->DOEPCTL = (0x40U & USB_OTG_DOEPCTL_MPSIZ) | (2UL << 18) | USB_OTG_DOEPCTL_SD0PID_SEVNFRM | USB_OTG_DOEPCTL_USBAEP;
      USBx_OUTEP(3U)->DOEPINT = 0xFF;
      USBx_OUTEP(2U)->DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_CNAK;
      USBx_OUTEP(3U)->DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_CNAK;
      USB_WritePacket(0, 0, 0);
      USBx_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK;
      break;
    case USB_REQ_SET_ADDRESS:
      USBx_DEVICE->DCFG |= ((setup.b.wValue.w & 0x7fU) << 4);
      USB_WritePacket(0, 0, 0);
      USBx_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK;
      break;
    case USB_REQ_GET_DESCRIPTOR:
      switch (setup.b.wValue.bw.lsb) {
        case USB_DESC_TYPE_DEVICE:
          device_desc[13] = hw_type;
          USB_WritePacket(device_desc, MIN(sizeof(device_desc), setup.b.wLength.w), 0);
          USBx_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK;
          break;
        case USB_DESC_TYPE_CONFIGURATION:
          USB_WritePacket(configuration_desc, MIN(sizeof(configuration_desc), setup.b.wLength.w), 0);
          USBx_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK;
          break;
        case USB_DESC_TYPE_DEVICE_QUALIFIER:
          USB_WritePacket(device_qualifier, MIN(sizeof(device_qualifier), setup.b.wLength.w), 0);
          USBx_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK;
          break;
        case USB_DESC_TYPE_STRING:
          switch (setup.b.wValue.bw.msb) {
            case STRING_OFFSET_LANGID: USB_WritePacket((uint8_t*)string_language_desc, MIN(sizeof(string_language_desc), setup.b.wLength.w), 0); break;
            case STRING_OFFSET_IMANUFACTURER: USB_WritePacket((uint8_t*)string_manufacturer_desc, MIN(sizeof(string_manufacturer_desc), setup.b.wLength.w), 0); break;
            case STRING_OFFSET_IPRODUCT: USB_WritePacket((uint8_t*)string_product_desc, MIN(sizeof(string_product_desc), setup.b.wLength.w), 0); break;
            case STRING_OFFSET_ISERIAL:
              response[0] = 0x02 + (12 * 4);
              response[1] = 0x03;
              for (int i = 0; i < 12; i++){
                uint8_t cc = ((uint8_t *)UID_BASE)[i];
                response[2 + (i * 4)] = to_hex_char((cc >> 4) & 0xFU);
                response[2 + (i * 4) + 1] = '\0';
                response[2 + (i * 4) + 2] = to_hex_char((cc >> 0) & 0xFU);
                response[2 + (i * 4) + 3] = '\0';
              }
              USB_WritePacket(response, MIN(response[0], setup.b.wLength.w), 0);
              break;
            case STRING_OFFSET_ICONFIGURATION: USB_WritePacket((uint8_t*)string_configuration_desc, MIN(sizeof(string_configuration_desc), setup.b.wLength.w), 0); break;
            case 238: USB_WritePacket((uint8_t*)string_238_desc, MIN(sizeof(string_238_desc), setup.b.wLength.w), 0); break;
            default: USB_WritePacket(0, 0, 0); break;
          }
          USBx_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK;
          break;
        case USB_DESC_TYPE_BINARY_OBJECT_STORE:
          USB_WritePacket(binary_object_store_desc, MIN(sizeof(binary_object_store_desc), setup.b.wLength.w), 0);
          USBx_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK;
          break;
        default:
          USB_WritePacket(0, 0, 0);
          USBx_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK;
          break;
      }
      break;
    case USB_REQ_GET_STATUS:
      response[0] = 0; response[1] = 0;
      USB_WritePacket((void*)&response, 2, 0);
      USBx_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK;
      break;
    case USB_REQ_SET_INTERFACE:
      current_int0_alt_setting = setup.b.wValue.w;
      USB_WritePacket(0, 0, 0);
      USBx_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK;
      break;
    case WEBUSB_VENDOR_CODE:
      USB_WritePacket(0, 0, 0);
      USBx_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK;
      break;
    case MS_VENDOR_CODE:
      switch (setup.b.wIndex.w) {
        case WINUSB_REQ_GET_DESCRIPTOR: USB_WritePacket_EP0((uint8_t*)winusb_20_desc, MIN(sizeof(winusb_20_desc), setup.b.wLength.w)); break;
        case WINUSB_REQ_GET_COMPATID_DESCRIPTOR: USB_WritePacket_EP0((uint8_t*)winusb_ext_compatid_os_desc, MIN(sizeof(winusb_ext_compatid_os_desc), setup.b.wLength.w)); break;
        case WINUSB_REQ_GET_EXT_PROPS_OS: USB_WritePacket_EP0((uint8_t*)winusb_ext_prop_os_desc, MIN(sizeof(winusb_ext_prop_os_desc), setup.b.wLength.w)); break;
        default: USB_WritePacket_EP0(0, 0);
      }
      break;
    default:
      control_req.request = setup.b.bRequest;
      control_req.param1 = setup.b.wValue.w;
      control_req.param2 = setup.b.wIndex.w;
      control_req.length = setup.b.wLength.w;
      resp_len = comms_control_handler(&control_req, response);
      USB_WritePacket(response, MIN(resp_len, setup.b.wLength.w), 0);
      USBx_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK;
  }
}

void usb_irqhandler(void) {
  static uint8_t usbdata[0x100] __attribute__((aligned(4)));
  unsigned int gintsts = USBx->GINTSTS;
  unsigned int gotgint = USBx->GOTGINT;
  unsigned int daint = USBx_DEVICE->DAINT;

  if ((gintsts & USB_OTG_GINTSTS_USBRST) != 0U) {
    usb_reset();
  }
  if ((gintsts & USB_OTG_GINTSTS_RXFLVL) != 0U) {
    volatile unsigned int rxst = USBx->GRXSTSP;
    int status = (rxst & USB_OTG_GRXSTSP_PKTSTS) >> 17;
    if (status == STS_DATA_UPDT) {
      int endpoint = (rxst & USB_OTG_GRXSTSP_EPNUM);
      int len = (rxst & USB_OTG_GRXSTSP_BCNT) >> 4;
      (void)USB_ReadPacket(&usbdata, len);
      if (endpoint == 2) { comms_endpoint2_write((uint8_t *) usbdata, len); }
      if (endpoint == 3) { outep3_processing = true; comms_can_write(usbdata, len); }
    } else if (status == STS_SETUP_UPDT) {
      (void)USB_ReadPacket(&setup, 8);
    }
  }
  if ((gintsts & USB_OTG_GINTSTS_BOUTNAKEFF) || (gintsts & USB_OTG_GINTSTS_GINAKEFF)) {
    USBx_DEVICE->DCTL |= USB_OTG_DCTL_CGONAK | USB_OTG_DCTL_CGINAK;
  }
  if ((gintsts & USB_OTG_GINTSTS_OEPINT) != 0U) {
    if ((USBx_OUTEP(2U)->DOEPINT & USB_OTG_DOEPINT_XFRC) != 0U) {
      USBx_OUTEP(2U)->DOEPTSIZ = (1UL << 19) | 0x40U;
      USBx_OUTEP(2U)->DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_CNAK;
    }
    if ((USBx_OUTEP(3U)->DOEPINT & USB_OTG_DOEPINT_XFRC) != 0U) {
      outep3_processing = false;
      refresh_can_tx_slots_available();
    }
    if ((USBx_OUTEP(0U)->DOEPINT & USB_OTG_DIEPINT_XFRC) != 0U) {
      USBx_OUTEP(0U)->DOEPTSIZ = USB_OTG_DOEPTSIZ_STUPCNT | (USB_OTG_DOEPTSIZ_PKTCNT & (1UL << 19)) | (1U << 3);
    }
    if ((USBx_OUTEP(0U)->DOEPINT & USB_OTG_DOEPINT_STUP) != 0U) {
      usb_setup();
    }
    USBx_OUTEP(0U)->DOEPINT = USBx_OUTEP(0U)->DOEPINT;
    USBx_OUTEP(2U)->DOEPINT = USBx_OUTEP(2U)->DOEPINT;
    USBx_OUTEP(3U)->DOEPINT = USBx_OUTEP(3U)->DOEPINT;
  }
  if ((gintsts & USB_OTG_GINTSTS_IEPINT) != 0U) {
    switch (current_int0_alt_setting) {
      case 0:
        if ((USBx_INEP(1U)->DIEPINT & USB_OTG_DIEPMSK_ITTXFEMSK) != 0U) {
          USB_WritePacket((void *)response, comms_can_read(response, 0x40), 1);
        }
        break;
      case 1:
        if ((USBx_INEP(1U)->DIEPINT & USB_OTG_DIEPMSK_ITTXFEMSK) != 0U) {
          int len = comms_can_read(response, 0x40);
          if (len > 0) { USB_WritePacket((void *)response, len, 1); }
        }
        break;
      default: break;
    }
    if ((USBx_INEP(0U)->DIEPINT & USB_OTG_DIEPMSK_ITTXFEMSK) != 0U) {
      if ((ep0_txlen != 0U) && ((USBx_INEP(0U)->DTXFSTS & USB_OTG_DTXFSTS_INEPTFSAV) >= 0x40U)) {
        uint16_t len = MIN(ep0_txlen, 0x40);
        USB_WritePacket(ep0_txdata, len, 0);
        ep0_txdata = &ep0_txdata[len];
        ep0_txlen -= len;
        if (ep0_txlen == 0U) {
          ep0_txdata = NULL;
          USBx_DEVICE->DIEPEMPMSK &= ~1;
          USBx_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK;
        }
      }
    }
    USBx_INEP(0U)->DIEPINT = USBx_INEP(0U)->DIEPINT;
    USBx_INEP(1U)->DIEPINT = USBx_INEP(1U)->DIEPINT;
  }
  USBx_DEVICE->DAINT = daint;
  USBx->GOTGINT = gotgint;
  USBx->GINTSTS = gintsts;
}

void can_tx_comms_resume_usb(void) {
  ENTER_CRITICAL();
  if (!outep3_processing && (USBx_OUTEP(3U)->DOEPCTL & USB_OTG_DOEPCTL_NAKSTS) != 0U) {
    USBx_OUTEP(3U)->DOEPTSIZ = (32UL << 19) | 0x800U;
    USBx_OUTEP(3U)->DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_CNAK;
  }
  EXIT_CRITICAL();
}
