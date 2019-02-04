#!/usr/bin/env python
import csv
import sys
import time
from panda import Panda

# Replays message logs saved from Cabana.
# Sends messages on the same bus and with the same timing that they were seen.
# Converts messages sent by Panda (bus 128+) to the actual bus number.

# format from Cabana's Save Log:
# time,addr,bus,data
# 0.06194250166663551,308,0,08007c630a00d064

# To exclude some messages, filter them out from the CSV first:
# fgrep -v ,658, from-cabana.csv > filtered.csv

# Then run it with:
# ./can_replay.py filtered.csv


print("Trying to connect to Panda over USB...")
p = Panda()
p.set_safety_mode(Panda.SAFETY_ALLOUTPUT)
p.can_clear(0)

# TODO loop until ignition is detected

prev_time = 0.0
with open(sys.argv[1]) as f:
  reader = csv.reader(f)
  for row in reader:
    (timestamp, address, bus, data) = row
    if 'time' == timestamp:
      continue  # skip CSV header
    timestamp = float(timestamp)
    time.sleep(timestamp - prev_time)
    prev_time = timestamp
    bus = int(bus)
    if bus >= 128:
      bus -= 128  # convert messages sent by panda to the actual bus
    p.can_send(int(address), data.decode('hex'), bus)
