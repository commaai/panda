#ifndef BOARD_DRIVERS_USB_H
#define BOARD_DRIVERS_USB_H

#include "board/drivers/drivers.h"

// IRQs: OTG_FS

typedef struct {
  uint8_t  bmRequestType;
  uint8_t  bRequest;
  uint16_t wValue;
  uint16_t wIndex;
  uint16_t wLength;
} USB_Setup_TypeDef;

// **** supporting defines ****
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

// offsets for configuration strings
#define  STRING_OFFSET_LANGID                           0x00
#define  STRING_OFFSET_IMANUFACTURER                    0x01
#define  STRING_OFFSET_IPRODUCT                         0x02
#define  STRING_OFFSET_ISERIAL                          0x03
#define  STRING_OFFSET_ICONFIGURATION                   0x04

// WinUSB requests
#define  WINUSB_REQ_GET_COMPATID_DESCRIPTOR             0x04
#define  WINUSB_REQ_GET_EXT_PROPS_OS                    0x05
#define  WINUSB_REQ_GET_DESCRIPTOR                      0x07

#define STS_DATA_UPDT                          2U
#define STS_SETUP_UPDT                         6U

// for the repeating interfaces
#define DSCR_INTERFACE_LEN 9
#define DSCR_ENDPOINT_LEN 7
#define DSCR_CONFIG_LEN 9
#define DSCR_DEVICE_LEN 18

// endpoint types
#define ENDPOINT_TYPE_BULK 2
#define ENDPOINT_TYPE_INT 3

// These are arbitrary values used in bRequest
#define  MS_VENDOR_CODE 0x20
#define  WEBUSB_VENDOR_CODE 0x30

// BOS constants
#define BINARY_OBJECT_STORE_DESCRIPTOR_LENGTH   0x05
#define BINARY_OBJECT_STORE_DESCRIPTOR          0x0F
#define WINUSB_PLATFORM_DESCRIPTOR_LENGTH       0x9E

// Convert machine byte order to USB byte order
#define TOUSBORDER(num)\
  ((num) & 0xFFU), (((uint16_t)(num) >> 8) & 0xFFU)

// take in string length (in UTF-16 code units) and return the first 2 bytes of a string descriptor
#define STRING_DESCRIPTOR_HEADER(len)\
  (((len) * 2U) + 2U), USB_DESC_TYPE_STRING

// one ASCII character as a descriptor byte, or as a UTF-16LE code unit (two bytes)
#define ASCII(c) ((uint8_t)(c))
#define UTF16LE(c) ASCII(c), 0x00U

#define ENDPOINT_RCV 0x80U
#define ENDPOINT_SND 0x00U

static uint8_t response[USBPACKET_MAX_SIZE];

// current packet
static USB_Setup_TypeDef setup;
static const uint8_t *ep0_txdata = NULL;
static uint32_t ep0_txlen = 0;
static bool outep3_processing = false;

// Store the current interface alt setting.
static uint16_t current_int0_alt_setting = 0U;

// packet read and write

static void USB_ReadPacket(uint32_t *dest, uint32_t len) {
  uint32_t count32b = (len + 3U) / 4U;

  for (uint32_t i = 0; i < count32b; i++) {
    dest[i] = USBx_DFIFO(0U);
  }
}

static void USB_WritePacket(const uint8_t *src, uint32_t len, uint32_t ep) {
  uint32_t numpacket = (len + (USBPACKET_MAX_SIZE - 1U)) / USBPACKET_MAX_SIZE;

  // TODO: revisit this
  USBx_INEP(ep)->DIEPTSIZ = ((numpacket << 19) & USB_OTG_DIEPTSIZ_PKTCNT) |
                            (len               & USB_OTG_DIEPTSIZ_XFRSIZ);
  USBx_INEP(ep)->DIEPCTL |= (USB_OTG_DIEPCTL_CNAK | USB_OTG_DIEPCTL_EPENA);

  // load the FIFO a little-endian word at a time, from bytes so src needs no alignment
  if (src != NULL) {
    uint32_t i = 0U;
    for (; (len - i) >= 4U; i += 4U) {
      USBx_DFIFO(ep) = ((uint32_t)src[i + 3U] << 24) | ((uint32_t)src[i + 2U] << 16) |
                       ((uint32_t)src[i + 1U] << 8) | src[i];
    }

    // then the last 1-3 bytes, zero padded
    if (i < len) {
      uint32_t word = 0U;
      for (uint32_t j = len; j > i; j--) {
        word = (word << 8) | src[j - 1U];
      }
      USBx_DFIFO(ep) = word;
    }
  }
}

// IN EP 0 TX FIFO has a max size of 127 bytes (much smaller than the rest)
// so use TX FIFO empty interrupt to send larger amounts of data
static void USB_WritePacket_EP0(const uint8_t *src, uint32_t len) {
  uint32_t wplen = min_u32(len, 0x40U);
  USB_WritePacket(src, wplen, 0);

  if (wplen < len) {
    ep0_txdata = &src[wplen];
    ep0_txlen = len - wplen;
    USBx_DEVICE->DIEPEMPMSK |= 1U;
  } else {
    USBx_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK;
  }
}

// send a descriptor, cut short if the host asked for less of it
static void usb_write_descriptor(const uint8_t *desc, uint32_t len) {
  USB_WritePacket(desc, min_u32(len, setup.wLength), 0);
}

static void usb_reset(void) {
  // unmask endpoint interrupts, so many sets
  USBx_DEVICE->DAINT = 0xFFFFFFFFU;
  USBx_DEVICE->DAINTMSK = 0xFFFFFFFFU;

  // all interrupts for debugging
  USBx_DEVICE->DIEPMSK = 0xFFFFFFFFU;
  USBx_DEVICE->DOEPMSK = 0xFFFFFFFFU;

  // clear interrupts
  USBx_INEP(0U)->DIEPINT = 0xFF;
  USBx_OUTEP(0U)->DOEPINT = 0xFF;

  // unset the address
  USBx_DEVICE->DCFG &= ~USB_OTG_DCFG_DAD;

  // set up USB FIFOs
  // RX start address is fixed to 0
  USBx->GRXFSIZ = 0x40;

  // 0x100 to offset past GRXFSIZ
  USBx->DIEPTXF0_HNPTXFSIZ = (0x40UL << 16) | 0x40U;

  // EP1, massive
  USBx->DIEPTXF[0] = (0x40UL << 16) | 0x80U;

  // flush TX fifo
  USBx->GRSTCTL = USB_OTG_GRSTCTL_TXFFLSH | USB_OTG_GRSTCTL_TXFNUM_4;
  while ((USBx->GRSTCTL & USB_OTG_GRSTCTL_TXFFLSH) == USB_OTG_GRSTCTL_TXFFLSH) {}
  // flush RX FIFO
  USBx->GRSTCTL = USB_OTG_GRSTCTL_RXFFLSH;
  while ((USBx->GRSTCTL & USB_OTG_GRSTCTL_RXFFLSH) == USB_OTG_GRSTCTL_RXFFLSH) {}

  // no global NAK
  USBx_DEVICE->DCTL |= USB_OTG_DCTL_CGINAK;

  // ready to receive setup packets
  USBx_OUTEP(0U)->DOEPTSIZ = USB_OTG_DOEPTSIZ_STUPCNT | (USB_OTG_DOEPTSIZ_PKTCNT & (1UL << 19)) | (3U << 3);
}

static char to_hex_char(uint8_t a) {
  char ret;
  if (a < 10U) {
    ret = '0' + a;
  } else {
    ret = 'a' + (a - 10U);
  }
  return ret;
}

static void usb_setup(void) {
  static const uint8_t device_desc[] = {
    DSCR_DEVICE_LEN, USB_DESC_TYPE_DEVICE, //Length, Type
    0x10, 0x02, // bcdUSB max version of USB supported (2.1)
    0xFF, 0xFF, 0xFF, 0x40, // Class, Subclass, Protocol, Max Packet Size
    TOUSBORDER(USB_VID), // idVendor
    TOUSBORDER(USB_PID), // idProduct
    0x00, 0x00, // bcdDevice
    0x01, 0x02, // Manufacturer, Product
    0x03, 0x01 // Serial Number, Num Configurations
  };

  static const uint8_t device_qualifier[] = {
    0x0a, USB_DESC_TYPE_DEVICE_QUALIFIER, //Length, Type
    0x10, 0x02, // bcdUSB max version of USB supported (2.1)
    0xFF, 0xFF, 0xFF, 0x40, // bDeviceClass, bDeviceSubClass, bDeviceProtocol, bMaxPacketSize0
    0x01, 0x00 // bNumConfigurations, bReserved
  };

  static const uint8_t configuration_desc[] = {
    DSCR_CONFIG_LEN, USB_DESC_TYPE_CONFIGURATION, // Length, Type,
    TOUSBORDER(0x0045U), // Total Len (uint16)
    0x01, 0x01, STRING_OFFSET_ICONFIGURATION, // Num Interface, Config Value, Configuration
    0xc0, 0x32, // Attributes, Max Power
    // interface 0 ALT 0
    DSCR_INTERFACE_LEN, USB_DESC_TYPE_INTERFACE, // Length, Type
    0x00, 0x00, 0x03, // Index, Alt Index idx, Endpoint count
    0XFF, 0xFF, 0xFF, // Class, Subclass, Protocol
    0x00, // Interface
      // endpoint 1, read CAN
      DSCR_ENDPOINT_LEN, USB_DESC_TYPE_ENDPOINT, // Length, Type
      ENDPOINT_RCV | 1U, ENDPOINT_TYPE_BULK, // Endpoint Num/Direction, Type
      TOUSBORDER(0x0040U), // Max Packet (0x0040)
      0x00, // Polling Interval (NA)
      // endpoint 2, bootstub flashing
      DSCR_ENDPOINT_LEN, USB_DESC_TYPE_ENDPOINT, // Length, Type
      ENDPOINT_SND | 2U, ENDPOINT_TYPE_BULK, // Endpoint Num/Direction, Type
      TOUSBORDER(0x0040U), // Max Packet (0x0040)
      0x00, // Polling Interval
      // endpoint 3, send CAN
      DSCR_ENDPOINT_LEN, USB_DESC_TYPE_ENDPOINT, // Length, Type
      ENDPOINT_SND | 3U, ENDPOINT_TYPE_BULK, // Endpoint Num/Direction, Type
      TOUSBORDER(0x0040U), // Max Packet (0x0040)
      0x00, // Polling Interval
    // interface 0 ALT 1
    DSCR_INTERFACE_LEN, USB_DESC_TYPE_INTERFACE, // Length, Type
    0x00, 0x01, 0x03, // Index, Alt Index idx, Endpoint count
    0XFF, 0xFF, 0xFF, // Class, Subclass, Protocol
    0x00, // Interface
      // endpoint 1, read CAN
      DSCR_ENDPOINT_LEN, USB_DESC_TYPE_ENDPOINT, // Length, Type
      ENDPOINT_RCV | 1U, ENDPOINT_TYPE_INT, // Endpoint Num/Direction, Type
      TOUSBORDER(0x0040U), // Max Packet (0x0040)
      0x05, // Polling Interval (5 frames)
      // endpoint 2, bootstub flashing
      DSCR_ENDPOINT_LEN, USB_DESC_TYPE_ENDPOINT, // Length, Type
      ENDPOINT_SND | 2U, ENDPOINT_TYPE_BULK, // Endpoint Num/Direction, Type
      TOUSBORDER(0x0040U), // Max Packet (0x0040)
      0x00, // Polling Interval
      // endpoint 3, send CAN
      DSCR_ENDPOINT_LEN, USB_DESC_TYPE_ENDPOINT, // Length, Type
      ENDPOINT_SND | 3U, ENDPOINT_TYPE_BULK, // Endpoint Num/Direction, Type
      TOUSBORDER(0x0040U), // Max Packet (0x0040)
      0x00, // Polling Interval
  };

  static const uint8_t string_language_desc[] = {
    STRING_DESCRIPTOR_HEADER(1U),
    TOUSBORDER(0x0409U) // american english
  };

  static const uint8_t string_manufacturer_desc[] = {
    STRING_DESCRIPTOR_HEADER(8U),
    UTF16LE('c'), UTF16LE('o'), UTF16LE('m'), UTF16LE('m'), UTF16LE('a'), UTF16LE('.'), UTF16LE('a'), UTF16LE('i')
  };

  static const uint8_t string_product_desc[] = {
    STRING_DESCRIPTOR_HEADER(5U),
    UTF16LE('p'), UTF16LE('a'), UTF16LE('n'), UTF16LE('d'), UTF16LE('a')
  };

  // a string containing the default configuration index
  static const uint8_t string_configuration_desc[] = {
    STRING_DESCRIPTOR_HEADER(2U),
    UTF16LE('0'), UTF16LE('1') // "01"
  };

  // WCID (auto install WinUSB driver)
  // https://github.com/pbatard/libwdi/wiki/WCID-Devices
  // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/winusb-installation#automatic-installation-of--winusb-without-an-inf-file
  // WinUSB 1.0 descriptors, this is mostly used by Windows XP
  static const uint8_t string_238_desc[] = {
    0x12, USB_DESC_TYPE_STRING, // bLength, bDescriptorType
    UTF16LE('M'), UTF16LE('S'), UTF16LE('F'), UTF16LE('T'), UTF16LE('1'), UTF16LE('0'), UTF16LE('0'), // qwSignature (MSFT100)
    MS_VENDOR_CODE, 0x00 // bMS_VendorCode, bPad
  };

  static const uint8_t winusb_ext_compatid_os_desc[] = {
    0x28, 0x00, 0x00, 0x00, // dwLength
    0x00, 0x01, // bcdVersion
    0x04, 0x00, // wIndex
    0x01, // bCount
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Reserved
    0x00, // bFirstInterfaceNumber
    0x00, // Reserved
    ASCII('W'), ASCII('I'), ASCII('N'), ASCII('U'), ASCII('S'), ASCII('B'), 0x00, 0x00, // compatible ID (WINUSB)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // subcompatible ID (none)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00 // Reserved
  };

  static const uint8_t winusb_ext_prop_os_desc[] = {
    0x8e, 0x00, 0x00, 0x00, // dwLength
    0x00, 0x01, // bcdVersion
    0x05, 0x00, // wIndex
    0x01, 0x00, // wCount
    // first property
    0x84, 0x00, 0x00, 0x00, // dwSize
    0x01, 0x00, 0x00, 0x00, // dwPropertyDataType
    0x28, 0x00, // wPropertyNameLength
    // bPropertyName (DeviceInterfaceGUID)
    UTF16LE('D'), UTF16LE('e'), UTF16LE('v'), UTF16LE('i'), UTF16LE('c'), UTF16LE('e'), UTF16LE('I'), UTF16LE('n'),
    UTF16LE('t'), UTF16LE('e'), UTF16LE('r'), UTF16LE('f'), UTF16LE('a'), UTF16LE('c'), UTF16LE('e'), UTF16LE('G'),
    UTF16LE('U'), UTF16LE('I'), UTF16LE('D'), 0, 0,
    0x4e, 0x00, 0x00, 0x00, // dwPropertyDataLength
    // bPropertyData ({CCE5291C-A69F-4995-A4C2-2AE57A51ADE9})
    UTF16LE('{'), UTF16LE('c'), UTF16LE('c'), UTF16LE('e'), UTF16LE('5'), UTF16LE('2'), UTF16LE('9'), UTF16LE('1'),
    UTF16LE('c'), UTF16LE('-'), UTF16LE('a'), UTF16LE('6'), UTF16LE('9'), UTF16LE('f'), UTF16LE('-'), UTF16LE('4'),
    UTF16LE('9'), UTF16LE('9'), UTF16LE('5'), UTF16LE('-'), UTF16LE('a'), UTF16LE('4'), UTF16LE('c'), UTF16LE('2'),
    UTF16LE('-'), UTF16LE('2'), UTF16LE('a'), UTF16LE('e'), UTF16LE('5'), UTF16LE('7'), UTF16LE('a'), UTF16LE('5'),
    UTF16LE('1'), UTF16LE('a'), UTF16LE('d'), UTF16LE('e'), UTF16LE('9'), UTF16LE('}'), 0, 0,
  };

  // Binary Object Store descriptor used to expose WebUSB (and more WinUSB) metadata
  // comments are from the wicg spec
  // References used:
  //   https://wicg.github.io/webusb/#webusb-platform-capability-descriptor
  //   https://github.com/sowbug/weblight/blob/192ad7a0e903542e2aa28c607d98254a12a6399d/firmware/webusb.c
  //   https://os.mbed.com/users/larsgk/code/USBDevice_WebUSB/file/1d8a6665d607/WebUSBDevice/
  static const uint8_t binary_object_store_desc[] = {
    // BOS header
    BINARY_OBJECT_STORE_DESCRIPTOR_LENGTH, // bLength, this is only the length of the header
    BINARY_OBJECT_STORE_DESCRIPTOR, // bDescriptorType
    0x39, 0x00, // wTotalLength (LSB, MSB)
    0x02, // bNumDeviceCaps (WebUSB + WinUSB)

    // -------------------------------------------------
    // WebUSB descriptor
    // header
      0x18, // bLength, Size of this descriptor. Must be set to 24.
      0x10, // bDescriptorType, DEVICE CAPABILITY descriptor
      0x05, // bDevCapabilityType, PLATFORM capability
      0x00, // bReserved, This field is reserved and shall be set to zero.

    // PlatformCapabilityUUID, Must be set to {3408b638-09a9-47a0-8bfd-a0768815b665}.
      0x38, 0xB6, 0x08, 0x34,
      0xA9, 0x09, 0xA0, 0x47,
      0x8B, 0xFD, 0xA0, 0x76,
      0x88, 0x15, 0xB6, 0x65,
    // </PlatformCapabilityUUID>

    0x00, 0x01, // bcdVersion, Protocol version supported. Must be set to 0x0100.
    WEBUSB_VENDOR_CODE, // bVendorCode, bRequest value used for issuing WebUSB requests.
    // there used to be a concept of "allowed origins", but it was removed from the spec
    // it was intended to be a security feature, but then the entire security model relies on domain ownership
    // https://github.com/WICG/webusb/issues/49
    // other implementations use various other indexed to leverate this no-longer-valid feature. we wont.
    // the spec says we *must* reply to index 0x03 with the url, so we'll hint that that's the right index
    0x03, // iLandingPage, URL descriptor index of the device’s landing page.

    // -------------------------------------------------
    // WinUSB descriptor
    // header
      0x1C, // Descriptor size (28 bytes)
      0x10, // Descriptor type (Device Capability)
      0x05, // Capability type (Platform)
      0x00, // Reserved

    // MS OS 2.0 Platform Capability ID (D8DD60DF-4589-4CC7-9CD2-659D9E648A9F)
    // Indicates the device supports the Microsoft OS 2.0 descriptor
      0xDF, 0x60, 0xDD, 0xD8,
      0x89, 0x45, 0xC7, 0x4C,
      0x9C, 0xD2, 0x65, 0x9D,
      0x9E, 0x64, 0x8A, 0x9F,

    0x00, 0x00, 0x03, 0x06, // Windows version, currently set to 8.1 (0x06030000)

    WINUSB_PLATFORM_DESCRIPTOR_LENGTH, 0x00, // MS OS 2.0 descriptor size (word)
    MS_VENDOR_CODE, 0x00 // vendor code, no alternate enumeration
  };

  // WinUSB 2.0 descriptor. This is what modern systems use
  // https://github.com/sowbug/weblight/blob/192ad7a0e903542e2aa28c607d98254a12a6399d/firmware/webusb.c
  // http://janaxelson.com/files/ms_os_20_descriptors.c
  // https://books.google.com/books?id=pkefBgAAQBAJ&pg=PA353&lpg=PA353
  static const uint8_t winusb_20_desc[WINUSB_PLATFORM_DESCRIPTOR_LENGTH] = {
    // Microsoft OS 2.0 descriptor set header (table 10)
    0x0A, 0x00, // Descriptor size (10 bytes)
    0x00, 0x00, // MS OS 2.0 descriptor set header

    0x00, 0x00, 0x03, 0x06, // Windows version (8.1) (0x06030000)
    WINUSB_PLATFORM_DESCRIPTOR_LENGTH, 0x00, // Total size of MS OS 2.0 descriptor set

    // Microsoft OS 2.0 compatible ID descriptor
      0x14, 0x00, // Descriptor size (20 bytes)
      0x03, 0x00, // MS OS 2.0 compatible ID descriptor
      ASCII('W'), ASCII('I'), ASCII('N'), ASCII('U'), ASCII('S'), ASCII('B'), 0x00, 0x00, // compatible ID (WINUSB)
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,     // Sub-compatible ID

    // Registry property descriptor
    0x80, 0x00, // Descriptor size (130 bytes)
    0x04, 0x00, // Registry Property descriptor
    0x01, 0x00, // Strings are null-terminated Unicode
    0x28, 0x00, // Size of Property Name (40 bytes) "DeviceInterfaceGUID"

    // bPropertyName (DeviceInterfaceGUID)
      UTF16LE('D'), UTF16LE('e'), UTF16LE('v'), UTF16LE('i'), UTF16LE('c'), UTF16LE('e'), UTF16LE('I'), UTF16LE('n'),
      UTF16LE('t'), UTF16LE('e'), UTF16LE('r'), UTF16LE('f'), UTF16LE('a'), UTF16LE('c'), UTF16LE('e'), UTF16LE('G'),
      UTF16LE('U'), UTF16LE('I'), UTF16LE('D'), 0x00, 0x00,

    0x4E, 0x00, // Size of Property Data (78 bytes)

    // Vendor-defined property data: {CCE5291C-A69F-4995-A4C2-2AE57A51ADE9}
      UTF16LE('{'), UTF16LE('c'), UTF16LE('c'), UTF16LE('e'), UTF16LE('5'), UTF16LE('2'), UTF16LE('9'), UTF16LE('1'), // 16
      UTF16LE('c'), UTF16LE('-'), UTF16LE('a'), UTF16LE('6'), UTF16LE('9'), UTF16LE('f'), UTF16LE('-'), UTF16LE('4'), // 32
      UTF16LE('9'), UTF16LE('9'), UTF16LE('5'), UTF16LE('-'), UTF16LE('a'), UTF16LE('4'), UTF16LE('c'), UTF16LE('2'), // 48
      UTF16LE('-'), UTF16LE('2'), UTF16LE('a'), UTF16LE('e'), UTF16LE('5'), UTF16LE('7'), UTF16LE('a'), UTF16LE('5'), // 64
      UTF16LE('1'), UTF16LE('a'), UTF16LE('d'), UTF16LE('e'), UTF16LE('9'), UTF16LE('}'), 0x00, 0x00 // 78 bytes
  };

  uint32_t resp_len;
  ControlPacket_t control_req;

  // setup packet is ready
  switch (setup.bRequest) {
    case USB_REQ_SET_CONFIGURATION:
      // enable other endpoints, has to be here?
      USBx_INEP(1U)->DIEPCTL = (0x40U & USB_OTG_DIEPCTL_MPSIZ) | (2UL << 18) | (1UL << 22) |
                              USB_OTG_DIEPCTL_SD0PID_SEVNFRM | USB_OTG_DIEPCTL_USBAEP;
      USBx_INEP(1U)->DIEPINT = 0xFF;

      USBx_OUTEP(2U)->DOEPTSIZ = (1UL << 19) | 0x40U;
      USBx_OUTEP(2U)->DOEPCTL = (0x40U & USB_OTG_DOEPCTL_MPSIZ) | (2UL << 18) |
                               USB_OTG_DOEPCTL_SD0PID_SEVNFRM | USB_OTG_DOEPCTL_USBAEP;
      USBx_OUTEP(2U)->DOEPINT = 0xFF;

      USBx_OUTEP(3U)->DOEPTSIZ = (32UL << 19) | 0x800U;
      USBx_OUTEP(3U)->DOEPCTL = (0x40U & USB_OTG_DOEPCTL_MPSIZ) | (2UL << 18) |
                               USB_OTG_DOEPCTL_SD0PID_SEVNFRM | USB_OTG_DOEPCTL_USBAEP;
      USBx_OUTEP(3U)->DOEPINT = 0xFF;

      // mark ready to receive
      USBx_OUTEP(2U)->DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_CNAK;
      USBx_OUTEP(3U)->DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_CNAK;

      USB_WritePacket(NULL, 0, 0);
      USBx_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK;
      break;
    case USB_REQ_SET_ADDRESS:
      // set now?
      USBx_DEVICE->DCFG |= (((uint32_t)setup.wValue & 0x7fU) << 4);

      USB_WritePacket(NULL, 0, 0);
      USBx_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK;

      break;
    case USB_REQ_GET_DESCRIPTOR:
      switch (setup.wValue >> 8) { // descriptor type
        case USB_DESC_TYPE_DEVICE:
          // send a copy with bcdDevice set to the hardware type
          (void)memcpy(response, device_desc, sizeof(device_desc));
          response[13] = hw_type;
          usb_write_descriptor(response, sizeof(device_desc));
          break;
        case USB_DESC_TYPE_CONFIGURATION:
          usb_write_descriptor(configuration_desc, sizeof(configuration_desc));
          break;
        case USB_DESC_TYPE_DEVICE_QUALIFIER:
          usb_write_descriptor(device_qualifier, sizeof(device_qualifier));
          break;
        case USB_DESC_TYPE_STRING:
          switch (setup.wValue & 0xFFU) { // descriptor index
            case STRING_OFFSET_LANGID:
              usb_write_descriptor(string_language_desc, sizeof(string_language_desc));
              break;
            case STRING_OFFSET_IMANUFACTURER:
              usb_write_descriptor(string_manufacturer_desc, sizeof(string_manufacturer_desc));
              break;
            case STRING_OFFSET_IPRODUCT:
              usb_write_descriptor(string_product_desc, sizeof(string_product_desc));
              break;
            case STRING_OFFSET_ISERIAL:
              response[0] = 0x02 + (12 * 4);
              response[1] = 0x03;

              // 96 bits = 12 bytes
              for (int i = 0; i < 12; i++){
                uint8_t cc = ((uint8_t *)UID_BASE)[i];
                response[2 + (i * 4)] = (uint8_t)to_hex_char((cc >> 4) & 0xFU);
                response[2 + (i * 4) + 1] = 0U;
                response[2 + (i * 4) + 2] = (uint8_t)to_hex_char((cc >> 0) & 0xFU);
                response[2 + (i * 4) + 3] = 0U;
              }

              usb_write_descriptor(response, response[0]);
              break;
            case STRING_OFFSET_ICONFIGURATION:
              usb_write_descriptor(string_configuration_desc, sizeof(string_configuration_desc));
              break;
            case 238:
              usb_write_descriptor(string_238_desc, sizeof(string_238_desc));
              break;
            default:
              // nothing
              USB_WritePacket(NULL, 0, 0);
              break;
          }
          break;
        case USB_DESC_TYPE_BINARY_OBJECT_STORE:
          usb_write_descriptor(binary_object_store_desc, sizeof(binary_object_store_desc));
          break;
        default:
          // nothing here?
          USB_WritePacket(NULL, 0, 0);
          break;
      }
      USBx_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK;
      break;
    case USB_REQ_GET_STATUS:
      // empty response?
      response[0] = 0;
      response[1] = 0;
      USB_WritePacket(response, 2, 0);
      USBx_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK;
      break;
    case USB_REQ_SET_INTERFACE:
      // Store the alt setting number for IN EP behavior.
      current_int0_alt_setting = setup.wValue;
      USB_WritePacket(NULL, 0, 0);
      USBx_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK;
      break;
    case WEBUSB_VENDOR_CODE:
      // probably asking for allowed origins, which was removed from the spec
      USB_WritePacket(NULL, 0, 0);
      USBx_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK;
      break;
    case MS_VENDOR_CODE:
      switch (setup.wIndex) {
        // winusb 2.0 descriptor from BOS
        case WINUSB_REQ_GET_DESCRIPTOR:
          USB_WritePacket_EP0(winusb_20_desc, min_u32(sizeof(winusb_20_desc), setup.wLength));
          break;
        // Extended Compat ID OS Descriptor
        case WINUSB_REQ_GET_COMPATID_DESCRIPTOR:
          USB_WritePacket_EP0(winusb_ext_compatid_os_desc, min_u32(sizeof(winusb_ext_compatid_os_desc), setup.wLength));
          break;
        // Extended Properties OS Descriptor
        case WINUSB_REQ_GET_EXT_PROPS_OS:
          USB_WritePacket_EP0(winusb_ext_prop_os_desc, min_u32(sizeof(winusb_ext_prop_os_desc), setup.wLength));
          break;
        default:
          USB_WritePacket_EP0(NULL, 0);
          break;
      }
      break;
    default:
      control_req.request = setup.bRequest;
      control_req.param1 = setup.wValue;
      control_req.param2 = setup.wIndex;
      control_req.length = setup.wLength;

      resp_len = comms_control_handler(&control_req, response);
      USB_WritePacket(response, min_u32(resp_len, setup.wLength), 0);
      USBx_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK;
      break;
  }
}

// ***************************** USB port *****************************

static void usb_irqhandler(void) {
  static uint32_t usbdata[0x40];
  unsigned int gintsts = USBx->GINTSTS;
  unsigned int gotgint = USBx->GOTGINT;
  unsigned int daint = USBx_DEVICE->DAINT;

  if ((gintsts & USB_OTG_GINTSTS_CIDSCHG) != 0U) {
    print("connector ID status change\n");
  }

  if ((gintsts & USB_OTG_GINTSTS_USBRST) != 0U) {
    usb_reset();
  }

  // RX FIFO first
  if ((gintsts & USB_OTG_GINTSTS_RXFLVL) != 0U) {
    // 1. Read the Receive status pop register
    volatile unsigned int rxst = USBx->GRXSTSP;
    unsigned int status = (rxst & USB_OTG_GRXSTSP_PKTSTS) >> 17;

    if (status == STS_DATA_UPDT) {
      unsigned int endpoint = (rxst & USB_OTG_GRXSTSP_EPNUM);
      uint32_t len = (rxst & USB_OTG_GRXSTSP_BCNT) >> 4;
      USB_ReadPacket(usbdata, len);
      if (endpoint == 2U) {
        comms_endpoint2_write((uint8_t *)usbdata, len);
      }

      if (endpoint == 3U) {
        outep3_processing = true;
        comms_can_write((uint8_t *)usbdata, len);
      }
    } else if (status == STS_SETUP_UPDT) {
      uint32_t packet[2];
      USB_ReadPacket(packet, sizeof(packet));
      setup.bmRequestType = (uint8_t)packet[0];
      setup.bRequest = (uint8_t)(packet[0] >> 8);
      setup.wValue = (uint16_t)(packet[0] >> 16);
      setup.wIndex = (uint16_t)packet[1];
      setup.wLength = (uint16_t)(packet[1] >> 16);
    } else {
      // status is neither STS_DATA_UPDT or STS_SETUP_UPDT, skip
    }
  }

  if ((gintsts & (USB_OTG_GINTSTS_BOUTNAKEFF | USB_OTG_GINTSTS_GINAKEFF)) != 0U) {
    // no global NAK, why is this getting set?
    USBx_DEVICE->DCTL |= USB_OTG_DCTL_CGONAK | USB_OTG_DCTL_CGINAK;
  }

  // out endpoint hit
  if ((gintsts & USB_OTG_GINTSTS_OEPINT) != 0U) {
    if ((USBx_OUTEP(2U)->DOEPINT & USB_OTG_DOEPINT_XFRC) != 0U) {
      USBx_OUTEP(2U)->DOEPTSIZ = (1UL << 19) | 0x40U;
      USBx_OUTEP(2U)->DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_CNAK;
    }

    if ((USBx_OUTEP(3U)->DOEPINT & USB_OTG_DOEPINT_XFRC) != 0U) {
      // NAK cleared by process_can (if tx buffers have room)
      outep3_processing = false;
      refresh_can_tx_slots_available();
    }

    if ((USBx_OUTEP(0U)->DOEPINT & USB_OTG_DIEPINT_XFRC) != 0U) {
      // ready for next packet
      USBx_OUTEP(0U)->DOEPTSIZ = USB_OTG_DOEPTSIZ_STUPCNT | (USB_OTG_DOEPTSIZ_PKTCNT & (1UL << 19)) | (1U << 3);
    }

    // respond to setup packets
    if ((USBx_OUTEP(0U)->DOEPINT & USB_OTG_DOEPINT_STUP) != 0U) {
      usb_setup();
    }

    USBx_OUTEP(0U)->DOEPINT = USBx_OUTEP(0U)->DOEPINT;
    USBx_OUTEP(2U)->DOEPINT = USBx_OUTEP(2U)->DOEPINT;
    USBx_OUTEP(3U)->DOEPINT = USBx_OUTEP(3U)->DOEPINT;
  }

  // interrupt endpoint hit (Page 1221)
  if ((gintsts & USB_OTG_GINTSTS_IEPINT) != 0U) {
    // Should likely check the EP of the IN request even if there is
    // only one IN endpoint.

    // No need to set NAK in OTG_DIEPCTL0 when nothing to send,
    // Appears USB core automatically sets NAK. WritePacket clears it.

    // Handle the two interface alternate settings. Setting 0 has EP1
    // as bulk. Setting 1 has EP1 as interrupt. The code to handle
    // these two EP variations are very similar and can be
    // restructured for smaller code footprint. Keeping split out for
    // now for clarity.

    switch (current_int0_alt_setting) {
      case 0U: ////// Bulk config
        // *** IN token received when TxFIFO is empty
        if ((USBx_INEP(1U)->DIEPINT & USB_OTG_DIEPMSK_ITTXFEMSK) != 0U) {
          // TODO: always assuming max len, can we get the length?
          USB_WritePacket(response, comms_can_read(response, 0x40), 1);
        }
        break;

      case 1U: ////// Interrupt config
        // *** IN token received when TxFIFO is empty
        if ((USBx_INEP(1U)->DIEPINT & USB_OTG_DIEPMSK_ITTXFEMSK) != 0U) {
          // TODO: always assuming max len, can we get the length?
          uint32_t len = comms_can_read(response, 0x40);
          if (len > 0U) {
            USB_WritePacket(response, len, 1);
          }
        }
        break;
      default:
        print("current_int0_alt_setting value invalid\n");
        break;
    }

    if ((USBx_INEP(0U)->DIEPINT & USB_OTG_DIEPMSK_ITTXFEMSK) != 0U) {
      bool fifo_space = (USBx_INEP(0U)->DTXFSTS & USB_OTG_DTXFSTS_INEPTFSAV) >= 0x40U;
      if ((ep0_txlen != 0U) && fifo_space) {
        uint32_t len = min_u32(ep0_txlen, 0x40U);
        USB_WritePacket(ep0_txdata, len, 0);
        ep0_txdata = &ep0_txdata[len];
        ep0_txlen -= len;
        if (ep0_txlen == 0U) {
          ep0_txdata = NULL;
          USBx_DEVICE->DIEPEMPMSK &= ~1U;
          USBx_OUTEP(0U)->DOEPCTL |= USB_OTG_DOEPCTL_CNAK;
        }
      }
    }

    // clear interrupts
    USBx_INEP(0U)->DIEPINT = USBx_INEP(0U)->DIEPINT; // Why ep0?
    USBx_INEP(1U)->DIEPINT = USBx_INEP(1U)->DIEPINT;
  }

  // clear all interrupts we handled
  USBx_DEVICE->DAINT = daint;
  USBx->GOTGINT = gotgint;
  USBx->GINTSTS = gintsts;
}

static inline void can_tx_comms_resume_usb(void) {
  ENTER_CRITICAL();
  bool nak = (USBx_OUTEP(3U)->DOEPCTL & USB_OTG_DOEPCTL_NAKSTS) != 0U;
  if (!outep3_processing && nak) {
    USBx_OUTEP(3U)->DOEPTSIZ = (32UL << 19) | 0x800U;
    USBx_OUTEP(3U)->DOEPCTL |= USB_OTG_DOEPCTL_EPENA | USB_OTG_DOEPCTL_CNAK;
  }
  EXIT_CRITICAL();
}

#endif  // BOARD_DRIVERS_USB_H
