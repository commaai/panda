# python helpers for the body panda
import struct

from panda import Panda


class PandaBody(Panda):
  SUPPORTED_DEVICES = (Panda.HW_TYPE_BODY,)

  def __init__(self, *args, **kwargs):
    super().__init__(*args, **kwargs)
    if self.get_type() not in PandaBody.SUPPORTED_DEVICES:
      raise ValueError("connected device is not a body panda")

  # ****************** Motor Control *****************

  def motor_set_speed(self, motor: int, speed: int) -> None:
    assert motor in (1, 2), "motor must be 1 or 2"
    assert -100 <= speed <= 100, "speed must be between -100 and 100"
    speed_param = speed if speed >= 0 else (256 + speed)
    self._handle.controlWrite(Panda.REQUEST_OUT, 0xe0, motor, speed_param, b'')

  def motor_set_target_rpm(self, motor: int, rpm: float) -> None:
    assert motor in (1, 2), "motor must be 1 or 2"
    target_deci_rpm = int(round(rpm * 10.0))
    assert -32768 <= target_deci_rpm <= 32767, "target rpm out of range (-3276.8 to 3276.7)"
    target_param = target_deci_rpm if target_deci_rpm >= 0 else (target_deci_rpm + (1 << 16))
    self._handle.controlWrite(Panda.REQUEST_OUT, 0xe4, motor, target_param, b'')

  def motor_stop(self, motor: int) -> None:
    assert motor in (1, 2), "motor must be 1 or 2"
    self._handle.controlWrite(Panda.REQUEST_OUT, 0xe1, motor, 0, b'')

  def motor_get_encoder_state(self, motor: int) -> tuple[int, float]:
    assert motor in (1, 2), "motor must be 1 or 2"
    dat = self._handle.controlRead(Panda.REQUEST_IN, 0xe2, motor, 0, 8)
    position, rpm_milli = struct.unpack("<ii", dat)
    return position, rpm_milli / 1000.0

  def motor_reset_encoder(self, motor: int) -> None:
    assert motor in (1, 2), "motor must be 1 or 2"
    self._handle.controlWrite(Panda.REQUEST_OUT, 0xe3, motor, 0, b'')
