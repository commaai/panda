#!/usr/bin/env python3
import os
import time
import concurrent.futures

from panda import Panda, PandaDFU, PandaJungle, PandaJungleDFU, McuType
from panda.tests.libs.resetter import Resetter

# Reset + flash all CI hardware to get it into a consistent state
# * port 1: jungles-under-test
# * port 2: USB hubs
# * port 3: HITL pandas and their jungles
if __name__ == "__main__":
  with Resetter() as r:
    # everything off
    for i in range(1, 4):
      r.enable_power(i, 0)

    # reflash test jungles
    r.cycle_power(delay=7, ports=[1, 2], dfu=True)
    dfu_serials = PandaJungleDFU.list()
    assert len(dfu_serials) == 2
    mcu_types = {}
    for s in dfu_serials:
      with PandaJungleDFU(s) as pd:
        mcu_types.add(pd.get_mcu_type())
        pd.recover()
    assert mcu_types == {McuType.F4, McuType.H7}
