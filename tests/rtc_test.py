#!/usr/bin/env python
import datetime

from panda import Panda

if __name__ == "__main__":
  p = Panda()

  p.set_datetime(datetime.datetime.now())
  print(p.get_datetime())
