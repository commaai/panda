# python library to interface with panda
import struct

import usb1
from usb1 import USBErrorIO, USBErrorOverflow

class Panda(object):
  def __init__(self, serial=None, claim=True):
    context = usb1.USBContext()

    self.handle = None
    for device in context.getDeviceList(skip_on_error=True):
      if device.getVendorID() == 0xbbaa and device.getProductID() == 0xddcc:
        if serial is None or device.getSerialNumber() == serial:
          print "opening device", device.getSerialNumber()
          self.handle = device.open()
          if claim:
            self.handle.claimInterface(0)
          break

    assert self.handle != None

  @staticmethod
  def list():
    context = usb1.USBContext()
    ret = []
    for device in context.getDeviceList(skip_on_error=True):
      if device.getVendorID() == 0xbbaa and device.getProductID() == 0xddcc:
        ret.append(device.getSerialNumber())
    return ret

  # ******************* health *******************

  def health(self):
    dat = self.handle.controlRead(usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE, 0xd2, 0, 0, 0x20)
    a = struct.unpack("IIBBBBB", dat)
    return {"voltage": a[0], "current": a[1],
            "started": a[2], "controls_allowed": a[3],
            "gas_interceptor_detected": a[4],
            "started_signal_detected": a[5],
            "started_alt": a[6]}

  # ******************* can *******************

  def set_gmlan(self, on):
    if on:
      self.handle.controlWrite(usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE, 0xdb, 1, 0, '')
    else:
      self.handle.controlWrite(usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE, 0xdb, 0, 0, '')

  def can_send_many(self, arr):
    snds = []
    for addr, _, dat, bus in arr:
      snd = struct.pack("II", ((addr << 21) | 1), len(dat) | (bus << 4)) + dat
      snd = snd.ljust(0x10, '\x00')
      snds.append(snd)

    while 1:
      try:
        self.handle.bulkWrite(3, ''.join(snds))
        break
      except (USBErrorIO, USBErrorOverflow):
        print "CAN: BAD SEND MANY, RETRYING"

  def can_send(self, addr, dat, bus):
    self.can_send_many([[addr, None, dat, bus]])

  def can_recv(self):
    def __parse_can_buffer(dat):
      ret = []
      for j in range(0, len(dat), 0x10):
        ddat = dat[j:j+0x10]
        f1, f2 = struct.unpack("II", ddat[0:8])
        ret.append((f1 >> 21, f2>>16, ddat[8:8+(f2&0xF)], (f2>>4)&0xf))
      return ret
    dat = ""
    while 1:
      try:
        dat = self.handle.bulkRead(1, 0x10*256)
        break
      except (USBErrorIO, USBErrorOverflow):
        print "CAN: BAD RECV, RETRYING"
    return __parse_can_buffer(dat)

  # ******************* serial *******************

  def serial_read(self, port_number):
    return self.handle.controlRead(usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE, 0xe0, port_number, 0, 0x100)

  def serial_write(self, port_number):
    return self.handle.bulkWrite(2, chr(port_number) + ln)

  # ******************* kline *******************

  # pulse low for wakeup
  def kline_wakeup(self):
    ret = self.handle.controlWrite(usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE, 0xf0, 0, 0, "")

  def kline_drain(self, bus=2):
    # drain buffer
    bret = ""
    while 1:
      ret = self.handle.controlRead(usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE, 0xe0, bus, 0, 0x100)
      if len(ret) == 0:
        break
      bret += str(ret)
    return bret

  def kline_ll_recv(self, cnt, bus=2):
    echo = ""
    while len(echo) != cnt:
      echo += str(self.handle.controlRead(usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE, 0xe0, bus, 0, cnt-len(echo)))
    return echo

  def kline_send(self, x, bus=2, checksum=True):
    def get_checksum(dat):
      result = 0
      result += sum(map(ord, dat))
      result = -result
      return chr(result&0xFF)

    self.kline_drain(bus=bus)
    if checksum:
      x += get_checksum(x)
    for i in range(0, len(x), 0x10):
      ts = x[i:i+0x10]
      self.handle.bulkWrite(2, chr(bus)+ts)
      echo = self.kline_ll_recv(len(ts), bus=bus)
      if echo != ts:
        print "**** ECHO ERROR %d ****" % i
        print echo.encode("hex")
        print ts.encode("hex")
    assert echo == ts

  def kline_recv(self, bus=2):
    msg = self.kline_ll_recv(2, bus=bus)
    msg += self.kline_ll_recv(ord(msg[1])-2, bus=bus)
    return msg

