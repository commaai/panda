#!/usr/bin/env python2

import os
import requests

from replay_drive import replay_drive, safety_modes
from openpilot_tools.lib.logreader import LogReader

BASE_URL = "https://commadataci.blob.core.windows.net/openpilotci/"

# (route, safety mode, param)
logs = [
  ("2e07163a1ba9a780|2019-06-06--09-36-50.bz2", "TOYOTA", 100)
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

