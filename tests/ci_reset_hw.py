#!/usr/bin/env python3
import concurrent.futures

from panda import Panda, PandaDFU
from panda.tests.libs.resetter import Resetter

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
  assert len(pandas) == 7

  with concurrent.futures.ProcessPoolExecutor(max_workers=len(pandas)) as exc:
    def flash(serial):
      with Panda(serial) as pf:
        if pf.bootstub:
          pf.flash()
    list(exc.map(flash, pandas, timeout=20))

  r.cycle_power(delay=0, ports=[1, 2])
  r.close()
