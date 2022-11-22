from panda import Panda, PandaDFU
from panda.tests.libs.resetter import Resetter


# resets power for both jungles(ports 1 and 2) and USB hubs(port 3)
# puts pandas into DFU mode and flashes bootstub + app
if __name__ == "__main__":
  r = Resetter()

  r.enable_boot(True)
  r.cycle_power(delay=5, ports=[1,2,3])
  r.enable_boot(False)

  pandas = PandaDFU.list()
  print(pandas)
  assert len(pandas) == 8

  for serial in pandas:
    p = PandaDFU(serial)
    p.recover()

  r.cycle_power(delay=5, ports=[1,2])

  pandas = Panda.list()
  print(pandas)
  assert len(pandas) == 8

  for serial in pandas:
    pf = Panda(serial)
    if pf.bootstub:
      pf.flash()
    pf.close()

  r.cycle_power(delay=0, ports=[1,2])
  r.close()
