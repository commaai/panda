#!/usr/bin/env python3

from panda.python.spi_dfu import PandaSpiDFU

if __name__ == "__main__":
  p = PandaSpiDFU('')

  print("Bootloader version", p.get_bootloader_version())
  print("MCU ID", p.get_id())

  print("erasing...")
  p.global_erase()
  print("done")

  p.program_bootstub()
  print("done")
