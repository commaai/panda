#!/usr/bin/env python3
from panda import Panda

def main():
  for p in Panda.list():
    pp = Panda(p)
    print(f"{pp.get_serial()[0]}: {pp.get_version()}")

if __name__ == "__main__":
  main()
