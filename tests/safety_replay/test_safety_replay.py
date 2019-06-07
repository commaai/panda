#!/usr/bin/env python2

import os
import requests

from replay_drive import replay_drive, safety_modes
from openpilot_tools.lib.logreader import LogReader

BASE_URL = "https://commadataci.blob.core.windows.net/openpilotci/"

# (route, safety mode, param)
logs = [
  ("09f75413d2466119|2019-05-31--07-34-50.bz2", "HONDA", 0), # HONDA.CIVIC
  ("095d5469458829d8|2019-05-30--16-29-42.bz2", "TOYOTA", 66), # TOYOTA.PRIUS
  ("f8a6dd2a138b15ad|2019-06-01--11-40-53.bz2", "GM", 0), # GM.VOLT
  ("f1b4c567731f4a1b|2018-06-06--14-43-46.bz2", "HONDA_BOSCH", 1), # HONDA.ACCORD
  ("10626f7208cd14d4|2019-05-31--11-45-23.bz2", "HYUNDAI", 0), # HYUNDAI.SANTA_FE
  ("c987eca8a79b421f|2019-05-08--16-50-04.bz2", "CHRYSLER", 0), # CHRYSLER.PACIFICA_2018_HYBRID
  ("791340bc01ed993d|2019-05-02--10-14-09.bz2", "SUBARU", 0), # SUBARU.IMPREZA
]

if __name__ == "__main__":
  for route, _, _ in logs:
    if not os.path.isfile(route):
      with open(route, "w") as f:
        f.write(requests.get(BASE_URL + route).content)

  failed = []
  for route, mode, param in logs:
    lr = LogReader(route)
    m = safety_modes.get(mode, mode)

    print "\nreplaying %s with safety mode %d and param %s" % (route, m, param)
    if not replay_drive(lr, m, int(param)):
      failed.append(route)

    for f in failed:
      print "\n**** failed on %s ****" % f
    assert len(failed) == 0, "\nfailed on %d logs" % len(failed)

