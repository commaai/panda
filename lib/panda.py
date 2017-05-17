# python library to interface with panda
import struct
import hashlib
import socket
import usb1
from usb1 import USBErrorIO, USBErrorOverflow

try:
  from hexdump import hexdump
except:
  pass

def parse_can_buffer(dat):
  ret = []
  for j in range(0, len(dat), 0x10):
    ddat = dat[j:j+0x10]
    f1, f2 = struct.unpack("II", ddat[0:8])
    extended = 4
    if f1 & extended:
      address = f1 >> 3
    else:
      address = f1 >> 21
    ret.append((address, f2>>16, ddat[8:8+(f2&0xF)], (f2>>4)&0xf))
  return ret

class PandaWifiStreaming(object):
  def __init__(self, ip="192.168.0.10", port=1338):
    self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    self.sock.sendto("hello", (ip, port))
    self.sock.setblocking(0)
    self.ip = ip
    self.port = port

  def can_recv(self):
    ret = []
    while 1:
      try:
        dat, addr = self.sock.recvfrom(0x200*0x10)
        if addr == (self.ip, self.port):
          ret += parse_can_buffer(dat)
      except socket.error:
        break
    return ret

# stupid tunneling of USB over wifi and SPI
class WifiHandle(object):
  def __init__(self, ip="192.168.0.10", port=1337):
    self.sock = socket.create_connection((ip, port))

  def __recv(self):
    ret = self.sock.recv(0x44)
    length = struct.unpack("I", ret[0:4])[0]
    return ret[4:4+length]

  def controlWrite(self, request_type, request, value, index, data, timeout=0):
    # ignore data in reply, panda doesn't use it
    return self.controlRead(request_type, request, value, index, 0, timeout)

  def controlRead(self, request_type, request, value, index, length, timeout=0):
    self.sock.send(struct.pack("HHBBHHH", 0, 0, request_type, request, value, index, length))
    return self.__recv()

  def bulkWrite(self, endpoint, data, timeout=0):
    assert len(data) <= 0x10
    self.sock.send(struct.pack("HH", endpoint, len(data))+data)
    self.__recv()  # to /dev/null

  def bulkRead(self, endpoint, length, timeout=0):
    self.sock.send(struct.pack("HH", endpoint, 0))
    return self.__recv()

  def close(self):
    self.sock.close()

class Panda(object):
  def __init__(self, serial=None, claim=True):
    if serial == "WIFI":
      self.handle = WifiHandle()
      print "opening WIFI device"
    else:
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

  def close(self):
    self.handle.close()

  @staticmethod
  def list():
    context = usb1.USBContext()
    ret = []
    for device in context.getDeviceList(skip_on_error=True):
      if device.getVendorID() == 0xbbaa and device.getProductID() == 0xddcc:
        ret.append(device.getSerialNumber())
    # TODO: detect if this is real
    #ret += ["WIFI"]
    return ret

  # ******************* health *******************

  def health(self):
    dat = self.handle.controlRead(usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE, 0xd2, 0, 0, 13)
    a = struct.unpack("IIBBBBB", dat)
    return {"voltage": a[0], "current": a[1],
            "started": a[2], "controls_allowed": a[3],
            "gas_interceptor_detected": a[4],
            "started_signal_detected": a[5],
            "started_alt": a[6]}

  # ******************* control *******************

  def enter_bootloader(self):
    try:
      self.handle.controlWrite(usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE, 0xd1, 0, 0, '')
    except Exception:
      pass

  def get_serial(self):
    dat = str(self.handle.controlRead(usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE, 0xd0, 0, 0, 0x20))
    assert dat[0x1c:] == hashlib.sha1(dat[0:0x1c]).digest()[0:4]
    return [dat[0:0x10], dat[0x10:0x10+10]]

  def get_secret(self):
    dat = str(self.handle.controlRead(usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE, 0xd0, 1, 0, 0x10))
    return dat.encode("hex")

  # ******************* configuration *******************

  def set_controls_allowed(self, on):
    if on:
      self.handle.controlWrite(usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE, 0xdc, 0x1337, 0, '')
    else:
      self.handle.controlWrite(usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE, 0xdc, 0, 0, '')

  def set_gmlan(self, on):
    if on:
      self.handle.controlWrite(usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE, 0xdb, 1, 0, '')
    else:
      self.handle.controlWrite(usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE, 0xdb, 0, 0, '')

  def set_uart_baud(self, uart, rate):
    self.handle.controlWrite(usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE, 0xe1, uart, rate, '')

  def set_uart_parity(self, uart, parity):
    # parity, 0=off, 1=even, 2=odd
    self.handle.controlWrite(usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE, 0xe2, uart, parity, '')

  def set_uart_callback(self, uart, install):
    self.handle.controlWrite(usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE, 0xe3, uart, int(install), '')

  # ******************* can *******************

  def can_send_many(self, arr):
    snds = []
    for addr, _, dat, bus in arr:
      transmit = 1
      extended = 4
      if addr >= 0x800:
        rir = (addr << 3) | transmit | extended
      else:
        rir = (addr << 21) | transmit
      snd = struct.pack("II", rir, len(dat) | (bus << 4)) + dat
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
    dat = ""
    while 1:
      try:
        dat = self.handle.bulkRead(1, 0x10*256)
        break
      except (USBErrorIO, USBErrorOverflow):
        print "CAN: BAD RECV, RETRYING"
    return parse_can_buffer(dat)

  # ******************* serial *******************

  def serial_read(self, port_number):
    return self.handle.controlRead(usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE, 0xe0, port_number, 0, 0x40)

  def serial_write(self, port_number, ln):
    return self.handle.bulkWrite(2, chr(port_number) + ln)

  # ******************* kline *******************

  # pulse low for wakeup
  def kline_wakeup(self):
    ret = self.handle.controlWrite(usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE, 0xf0, 0, 0, "")

  def kline_drain(self, bus=2):
    # drain buffer
    bret = ""
    while 1:
      ret = self.handle.controlRead(usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE, 0xe0, bus, 0, 0x40)
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
    for i in range(0, len(x), 0xf):
      ts = x[i:i+0xf]
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

