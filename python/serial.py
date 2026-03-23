# mimic a python serial port
class PandaSerial:
  def __init__(self, panda, port, baud):
    self.panda = panda
    self.port = port
    self._baudrate = baud
    self.buf = b""

  def read(self, l=1):
    tt = self.panda.serial_read(self.port)
    if len(tt) > 0:
      self.buf += tt
    ret = self.buf[0:l]
    self.buf = self.buf[l:]
    return ret

  def write(self, dat):
    return self.panda.serial_write(self.port, dat)

  def close(self):
    pass

  def flush(self):
    pass

  @property
  def baudrate(self):
    return self._baudrate

  @baudrate.setter
  def baudrate(self, value):
    self._baudrate = value
