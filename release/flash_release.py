#!/usr/bin/env python
from __future__ import print_function
import sys
import time
from panda import Panda, PandaDFU, ESPROM, CesantaFlasher
from zipfile import ZipFile

def status(x):
  print("\033[1;32;40m"+x+"\033[00m")

if __name__ == "__main__":
  zf = ZipFile(sys.argv[1])
  zf.printdir()

  version = zf.read("version")

  code_bootstub = zf.read("bootstub.panda.bin")
  code_panda = zf.read("panda.bin")

  code_boot_15 = zf.read("boot_v1.5.bin")
  code_boot_15 = code_boot_15[0:2] + "\x00\x30" + code_boot_15[4:]

  code_user1 = zf.read("user1.bin")
  code_user2 = zf.read("user2.bin")
  
  # look for Panda
  st_serial = Panda.list()[0]

  # enter DFU mode
  status("1. Entering DFU mode")
  panda = Panda(st_serial)
  panda.enter_bootloader()
  time.sleep(1)

  # program bootstub
  status("2. Programming bootstub")
  dfu = PandaDFU(PandaDFU.st_serial_to_dfu_serial(st_serial))
  dfu.clear_status()
  dfu.erase(0x8004000)
  dfu.erase(0x8000000)
  dfu.program(0x8000000, code_bootstub, 0x800)
  dfu.reset()
  time.sleep(1)

  # flash main code
  status("3. Flashing main code")
  panda = Panda(st_serial)
  panda.flash(code=code_panda)
  panda.close()
  time.sleep(1)

  # flashing ESP
  status("4. Flashing ESP (slow!)")
  align = lambda x, sz=0x1000: x+"\xFF"*((sz-len(x)) % sz)
  esp = ESPROM(st_serial)
  esp.connect()
  flasher = CesantaFlasher(esp, 230400)
  flasher.flash_write(0x0, align(code_boot_15), True)
  flasher.flash_write(0x1000, align(code_user1), True)
  flasher.flash_write(0x81000, align(code_user2), True)
  flasher.flash_write(0x3FE000, "\xFF"*0x1000)
  flasher.boot_fw()
  del flasher
  del esp
  time.sleep(1)

  # check for connection
  status("5. Verifying version")
  panda = Panda(st_serial)
  my_version = panda.get_version()
  print(my_version, "should be", version)
  assert(str(version) == str(my_version))

  # done!
  status("6. Success!")
  

