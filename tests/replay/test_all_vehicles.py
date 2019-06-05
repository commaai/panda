#!/usr/bin/env python2

from openpilot_tools.lib.logreader import LogReader
from replay_drive import replay_drive, safety_modes


# TODO: download test routes for each vehicle

# (route, safety mode, param)
drives = [
  ("2e07163a1ba9a780|2019-06-01--15-45-44.bz2", 2, 100) # toyota test drive
]


if __name__ == "__main__":
  for route, mode, param in drives:
    lr = LogReader(route)
    m = safety_modes.get(mode, mode)
    assert replay_drive(lr, m, param), "replay failed on %s" % route

