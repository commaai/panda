from panda import Panda, PandaDFU
from panda.tests.libs.resetter import Resetter


if __name__ == "__main__":
  r = Resetter()

  r.enable_boot(True)
  r.cycle_power(5)
  r.enable_boot(False)

  pandas = PandaDFU.list()
  print(pandas)
  assert len(pandas) == 7

  for serial in pandas:
    p = PandaDFU(serial)
    p.recover()

  r.cycle_power(5)

  pandas = Panda.list()
  print(pandas)
  assert len(pandas) == 7

  for serial in pandas:
    pf = Panda(serial)
    if pf.bootstub:
      pf.flash()
    pf.close()

  r.cycle_power(0)
  r.close()
