#!/usr/bin/env python3
import os
import sys
import time
import _thread

sys.path.append(os.path.join(os.path.dirname(os.path.realpath(__file__)), ".."))
from python import Panda  # noqa: E402

# This script is intended to be used in conjunction with the echo_loopback_test.py test script from panda jungle.
# It sends a reversed response back for every message received containing b"test".

def heartbeat_thread(p):
  while True:
    try:
      p.send_heartbeat()
      time.sleep(0.5)
    except Exception:
      continue

# Resend every CAN message that has been received on the same bus, but with the data reversed
if __name__ == "__main__":
  pandas = Panda.list()
  if len(pandas) < 2:
    raise Exception("Need minimum 2 pandas!")
  print(f"Found pandas, using second in the list {pandas[0]}")
  p = Panda(pandas[0])

  _thread.start_new_thread(heartbeat_thread, (p,))
  p.set_can_speed_kbps(0, 1000)
  p.set_can_speed_kbps(1, 1000)
  p.set_can_speed_kbps(2, 1000)
  #p.set_can_data_speed_kbps(0, 5000)
  p.set_can_data_speed_kbps(1, 1000)
  p.set_can_data_speed_kbps(2, 5000)
  p.set_safety_mode(Panda.SAFETY_ALLOUTPUT) #SAFETY_ELM327
  p.set_power_save(False)

  msgs={0:0, 1:0, 2:0, 128:0, 129:0, 130:0}
  elapsed_s=0
  thr = 0
  TIMEOUT=1000
  PRINT_MSGS = False
  SEND_BACK = False

  print(p.get_canfd_status(0))
  print(p.get_canfd_status(1))
  print(p.get_canfd_status(2))

  while True:
    start_time = time.time()
    recv_msgs = []
    while True:
        incoming = p.can_recv()
        if not incoming: break
        recv_msgs.extend([[r[0]-1, r[1], r[2], r[3]] for r in incoming if r[3] < 3])
        for message in incoming:
          address, unused, data, bus = message
          msgs[bus]+=1
          if PRINT_MSGS: print(address, unused, data, bus)
    if SEND_BACK:
      p.can_send_many(recv_msgs, timeout=TIMEOUT)
    #time.sleep(0.005)
    elapsed_s += time.time()-start_time
    if elapsed_s >= 1:
      total = msgs[0] + msgs[1] + msgs[2]
      print(f"Received: {msgs}, speed: {int((total-thr)*(5+8) / 1024 / elapsed_s)} KBps")
      elapsed_s=0
      thr = total

