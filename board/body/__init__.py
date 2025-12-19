# python helpers for the body panda
import struct

from panda import Panda

class PandaBody(Panda):

  MOTOR_LEFT: int = 1
  MOTOR_RIGHT: int = 2

  def __init__(self, *args, **kwargs):
    super().__init__(*args, **kwargs)
    self._rpml = 0
    self._rpmr = 0

  @property
  def rpml(self) -> int:
    return self._rpml

  @rpml.setter
  def rpml(self, value: int) -> None:
    self._rpml = int(value)
    self._handle.controlWrite(Panda.REQUEST_OUT, 0xb3, self._rpml, self._rpmr, b'')

  @property
  def rpmr(self) -> int:
    return self._rpmr

  @rpmr.setter
  def rpmr(self, value: int) -> None:
    self._rpmr = int(value)
    self._handle.controlWrite(Panda.REQUEST_OUT, 0xb3, self._rpml, self._rpmr, b'')

  # ****************** Motor Control *****************
  @staticmethod
  def _ensure_valid_motor(motor: int) -> None:
    if motor not in (PandaBody.MOTOR_LEFT, PandaBody.MOTOR_RIGHT):
      raise ValueError("motor must be MOTOR_LEFT or MOTOR_RIGHT")

  def motor_get_encoder_state(self, motor: int) -> tuple[int, float]:
    self._ensure_valid_motor(motor)
    dat = self._handle.controlRead(Panda.REQUEST_IN, 0xe2, motor, 0, 8)
    position, rpm_milli = struct.unpack("<ii", dat)
    return position, rpm_milli / 1000.0
