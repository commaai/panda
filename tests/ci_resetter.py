import time
import usb1
from panda import Panda, PandaDFU


class Resetter():
  def __init__(self):
    self._handle = None
    self.connect()

  def close(self):
    self._handle.close()
    self._handle = None

  def connect(self):
    if self._handle:
      self.close()

    context = usb1.USBContext()
    self._handle = None

    while True:
      try:
        for device in context.getDeviceList(skip_on_error=True):
          if device.getVendorID() == 0xbbaa and device.getProductID() == 0xddc0:
            try:
              self._handle = device.open()
              self._handle.claimInterface(0)
              break
            except Exception as e:
              print(e)
              continue
      except Exception as e:
        print(e)
      if self._handle:
        break
      context = usb1.USBContext()
    assert self._handle

  def enable_power(self, port, enabled):
    self._handle.controlWrite((usb1.ENDPOINT_OUT | usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE), 0xff, port, enabled, b'')

  def enable_boot(self, enabled):
    self._handle.controlWrite((usb1.ENDPOINT_OUT | usb1.TYPE_VENDOR | usb1.RECIPIENT_DEVICE), 0xff, 0, enabled, b'')

  def cycle_power(self, delay=5):
    self.enable_power(1, False)
    self.enable_power(2, False)
    self.enable_power(3, False)
    time.sleep(1)
    self.enable_power(1, True)
    self.enable_power(2, True)
    self.enable_power(3, True)
    if delay > 0:
      time.sleep(delay)

if __name__ == "__main__":
  r = Resetter()

  r.enable_boot(True)
  r.cycle_power(5)
  r.enable_boot(False)

  pandas = PandaDFU.list()
  print(pandas)
  assert len(pandas) == 7

  for serial in pandas:
    p = PandaDFU(serial)
    p.recover()

  r.cycle_power(5)

  pandas = Panda.list()
  print(pandas)
  assert len(pandas) == 7

  for serial in pandas:
    pf = Panda(serial)
    if pf.bootstub:
      pf.flash()
    pf.close()

  r.cycle_power(0)
  r.close()
