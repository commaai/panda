#!/usr/bin/env python
import sys
from panda import Panda

def tesla_tester():
  
  try:
    print("Trying to connect to Panda over USB...")
    p = Panda()
    
  except AssertionError:
    print("USB connection failed. Trying WiFi...")
    
    try:
      p = Panda("WIFI")
    except:
      print("WiFi connection timed out. Please make sure your Panda is connected and try again.")
      sys.exit(0)

  bus_speed = 125 # Tesla Body busses (B, BF) are 125kbps, rest are 500kbps
  bus_num = 1 # My TDC to OBD adapter has PT on bus0 BDY on bus1 and CH on bus2
  p.set_can_speed_kbps(bus_num, bus_speed)
  
  # Now set the panda from its default of SAFETY_NOOUTPUT (read only) to SAFETY_ALLOUTPUT
  # Careful, as this will let us send any CAN messages we want (which could be very bad!)
  print("Setting Panda to output mode...")
  p.set_safety_mode(Panda.SAFETY_ALLOUTPUT)
  
  # BDY 0x248 is the MCU_commands message, which includes folding mirrors, opening the trunk, frunk, setting the cars lock state and more. For our test, we will edit the 3rd byte, which is MCU_lockRequest. 0x01 will lock, 0x02 will unlock:
  print("Unlocking Tesla...")
  p.can_send(0x248, "\x00\x00\x02\x00\x00\x00\x00\x00", bus_num)

  #Or, we can set the first byte, MCU_frontHoodCommand + MCU_liftgateSwitch, to 0x01 to pop the frunk, or 0x04 to open/close the trunk (0x05 should open both)
  print("Opening Frunk...")
  p.can_send(0x248, "\x01\x00\x00\x00\x00\x00\x00\x00", bus_num)
  
  #Back to safety...
  print("Disabling output on Panda...")
  p.set_safety_mode(Panda.SAFETY_NOOUTPUT)
  
if __name__ == "__main__":
  tesla_tester()