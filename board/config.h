#ifndef PANDA_CONFIG_H
#define PANDA_CONFIG_H

#include "rev.h"

//#define DEBUG
//#define DEBUG_USB

#define USB_VID 0xbbaa
#define USB_PID 0xddcc

#define CAN_DEFAULT_BITRATE 500000 // 500 khz
#define GMLAN_DEFAULT_BITRATE 33333 // 33.333 khz

#define FIFO_SIZE 0x100

#define NULL ((void*)0)

#define PANDA_SAFETY

#endif
