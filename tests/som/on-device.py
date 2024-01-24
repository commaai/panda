#!/usr/bin/env python3
import os
import time

from panda import Panda
from panda.tests.reflash_internal_panda import reflash_bootstub

if __name__ == "__main__":
  flag_set = False
  no_panda_count = 0
  while True:
    try:
      with Panda(disable_checks=False) as p:
        if not flag_set:
          p.set_heartbeat_disabled()
          p.set_safety_mode(Panda.SAFETY_ELM327, 30)
          flag_set = True

        # shutdown when told
        dt = p.get_datetime()
        if dt.year == 2040 and dt.month == 8:
          os.system("sudo poweroff")
        no_panda_count = 0
    except Exception as e:
      print(str(e))
      no_panda_count += 1
      if no_panda_count >= 5:
        reflash_bootstub()
        no_panda_count = 0
    time.sleep(0.5)
