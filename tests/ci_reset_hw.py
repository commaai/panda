#!/usr/bin/env python3
import os
import time
import concurrent.futures

from panda import Panda, PandaDFU, PandaJungle, PandaJungleDFU
from panda.tests.libs.resetter import Resetter

# all jungles used in the HITL tests
JUNGLES = [
  "058010800f51363038363036",
  "23002d000851393038373731"
]

# Reset + flash all CI hardware to get it into a consistent state
# * port 1: jungles-under-test
# * port 2: USB hubs
# # port 3: HITL pandas and their jungles
if __name__ == "__main__":
  r = Resetter()

  # everything off
  for i in range(1, 4):
    r.enable_power(i, 0)

  if "JUNGLE_TEST" in os.environ:
    # reflash test jungles
    r.cycle_power(delay=7, ports=[1, 2], dfu=True)
    dfu_serials = PandaJungleDFU.list()
    assert len(dfu_serials) == 2
    for s in dfu_serials:
      PandaJungleDFU(s).recover()

  # TODO: do this only after jungle tests pass
  # reflash panda support jungles
  r.cycle_power(delay=7, ports=[2, 3], dfu=True)
  js = PandaJungle.list()
  assert len(js) == 5
  for s in js:
    with PandaJungle(s) as pj:
      pj.set_panda_power(False)
      for i in range(8):
        pj.set_header_pin(i, 1)
      time.sleep(0.5)
      pj.set_panda_power(True)
  exit(0)

  with concurrent.futures.ProcessPoolExecutor(max_workers=len(pandas)) as exc:
    def recover(serial):
      PandaDFU(serial).recover()
    list(exc.map(recover, pandas, timeout=20))

  r.cycle_power(delay=7, ports=[1, 2])

  pandas = Panda.list()
  print(pandas)
  assert len(pandas) >= 7

  with concurrent.futures.ProcessPoolExecutor(max_workers=len(pandas)) as exc:
    def flash(serial):
      with Panda(serial) as pf:
        if pf.bootstub:
          pf.flash()
    list(exc.map(flash, pandas, timeout=20))

  print("flashing jungle")
  # flash jungles
  pjs = PandaJungle.list()
  assert set(pjs) == set(JUNGLES), f"Got different jungles than expected:\ngot      {set(pjs)}\nexpected {set(JUNGLES)}"
  for s in pjs:
    with PandaJungle(serial=s) as pj:
      print(f"- flashing {s}")
      pj.flash()

  r.cycle_power(delay=0, ports=[1, 2])
  r.close()
