from panda import Panda
from .helpers import test_all_pandas, panda_connect_and_init

@test_all_pandas
@panda_connect_and_init
def test_canfd_safety_modes(p):
  # works on all pandas
  p.set_safety_mode(Panda.SAFETY_ELM327)
  assert p.health()['safety_mode'] == Panda.SAFETY_ELM327

  # shouldn't be able to set a CAN-FD safety mode on non CAN-FD panda
  p.set_safety_mode(Panda.SAFETY_HYUNDAI_CANFD)
  expected_mode = Panda.SAFETY_HYUNDAI_CANFD if p.get_type() in Panda.H7_DEVICES else Panda.SAFETY_ELM327
  assert p.health()['safety_mode'] == expected_mode
