#!/usr/bin/env python3
import concurrent.futures

from panda import Panda, PandaDFU, PandaJungle
from panda.tests.libs.resetter import Resetter

# all jungles used in the HITL tests
JUNGLES = [
  "058010800f51363038363036",
  "23002d000851393038373731"
]


# Reset + flash all CI hardware to get it into a consistent state
# * ports 1-2 are jungles
# * port 3 is for the USB hubs
if __name__ == "__main__":
  r = Resetter()

  r.enable_boot(True)
  r.cycle_power(delay=7, ports=[1, 2, 3])
  r.enable_boot(False)

  pandas = PandaDFU.list()
  print("DFU pandas:", pandas)
  assert len(pandas) == 7

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
