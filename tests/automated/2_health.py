import time
from panda_jungle import PandaJungle  # pylint: disable=import-error
from .helpers import panda_jungle, test_all_pandas, test_all_gen2_pandas, panda_connect_and_init

@test_all_pandas
@panda_connect_and_init
def test_ignition(p):
  # Set harness orientation to #2, since the ignition line is on the wrong SBU bus :/
  panda_jungle.set_harness_orientation(PandaJungle.HARNESS_ORIENTATION_2)
  p.reset()

  for ign in (True, False):
    panda_jungle.set_ignition(ign)
    time.sleep(2)
    assert p.health()['ignition_line'] == ign


@test_all_gen2_pandas
@panda_connect_and_init
def test_orientation_detection(p):
  seen_orientations = []
  for i in range(3):
    panda_jungle.set_harness_orientation(i)
    time.sleep(0.25)
    p.reset()
    time.sleep(0.25)

    detected_harness_orientation = p.health()['car_harness_status']
    print(f"Detected orientation: {detected_harness_orientation}")
    if (i == 0 and detected_harness_orientation != 0) or detected_harness_orientation in seen_orientations:
      assert False
    seen_orientations.append(detected_harness_orientation)

@test_all_pandas
@panda_connect_and_init(full_reset=False)
def test_voltage(p):
  for _ in range(10):
    voltage = p.health()['voltage']
    assert ((voltage > 10000) and (voltage < 14000))
    time.sleep(0.1)
