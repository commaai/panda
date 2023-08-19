#!/usr/bin/env python3

from panda import Panda

if __name__ == "__main__":
  p = Panda()
  for l in p.get_logs():
    print(f"{l['id']:<6d} {l['timestamp']} {l['uptime']:6d} - {l['msg']}")
