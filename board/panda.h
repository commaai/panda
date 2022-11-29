// USB definitions
#define USB_VID 0xBBAAU

#ifdef BOOTSTUB
  #define USB_PID 0xDDEEU
#else
  #define USB_PID 0xDDCCU
#endif

#define USBPACKET_MAX_SIZE 0x40U

#define MAX_CAN_MSGS_PER_BULK_TRANSFER 51U
#define MAX_EP1_CHUNK_PER_BULK_TRANSFER 16256U // max data stream chunk in bytes, shouldn't be higher than 16320 or counter will overflow

#define CAN_INIT_TIMEOUT_MS 500U
