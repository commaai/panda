import time
import usb1


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

  def cycle_power(self, delay=5, ports=None):
    if ports is None:
      ports = [1, 2, 3]

    for port in ports:
      self.enable_power(port, False)

    time.sleep(1)

    for port in ports:
      self.enable_power(port, True)

    time.sleep(delay)
