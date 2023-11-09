#!/usr/bin/env python3
import os
import time

from panda import Panda


if __name__ == "__main__":
  flag_set = False
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
    except Exception as e:
      print(str(e))
    time.sleep(0.5)
