#!/usr/bin/env python2

import os
import requests

from replay_drive import replay_drive, safety_modes
from openpilot_tools.lib.logreader import LogReader

BASE_URL = "https://commadataci.blob.core.windows.net/openpilotci/"

# (route, safety mode, param)
logs = [
  ("", "HONDA", 100),
  ("", "TOYOTA", 100),
  ("", "GM", 100),
  ("", "HONDA_BOSCH", 100),
  ("", "HYUNDAI", 100),
  ("", "CHRYSLER", 100),
  ("", "SUBARU", 100),

]


if __name__ == "__main__":
  for route, _, _ in logs:
    if not os.path.isfile(route):
      with open(route, "w") as f:
        f.write(requests.get(BASE_URL + route).content)

  for route, mode, param in logs:
    lr = LogReader(route)
    m = safety_modes.get(mode, mode)

    print "replaying %s with safety mode %d and param %s" % (route, m, param)
    assert replay_drive(lr, m, int(param)), "replay failed on %s" % route

