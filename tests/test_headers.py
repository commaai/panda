import subprocess
import unittest
from pathlib import Path

import opendbc


ROOT = Path(__file__).resolve().parents[1]
INTERFACES = [f"board/{name}.h" for name in (
  "config", "main", "comms", "utils", "drivers/drivers", "sys/sys", "boards/boards", "stm32h7/stm32h7",
)]


class TestHeaders(unittest.TestCase):
  def check_headers(self, headers, defines=()):
    command = [
      "arm-none-eabi-gcc", "-fsyntax-only", "-x", "c", "-std=gnu11", "-Wall", "-Wextra", "-Werror",
      "-Wstrict-prototypes", "-fno-builtin", "-mcpu=cortex-m7", "-mthumb", "-DSTM32H7", "-DSTM32H725xx", "-DALLOW_DEBUG",
      "-DHEALTH_PACKET_VERSION=1", "-DCAN_PACKET_VERSION_HASH=1", "-DJUNGLE_HEALTH_PACKET_VERSION=1",
      "-I.", "-Iboard/stm32h7/inc", f"-I{opendbc.INCLUDE_PATH}",
      *(f"-D{define}" for define in defines), "-",
    ]
    source = "".join(f'#include "{header}"\n' for header in headers)
    result = subprocess.run(command, input=source, text=True, capture_output=True, cwd=ROOT)
    self.assertEqual(result.returncode, 0, result.stderr)

  def test_interfaces(self):
    # Interfaces must provide their own dependencies, regardless of include order.
    for variant in ((), ("PANDA_BODY",), ("PANDA_JUNGLE",)):
      for bootstub in ((), ("BOOTSTUB",)):
        defines = variant + bootstub
        for headers in ([header] for header in INTERFACES):
          with self.subTest(headers=headers, defines=defines):
            self.check_headers(headers, defines)
        for headers in (INTERFACES, INTERFACES[::-1]):
          with self.subTest(headers=headers, defines=defines):
            self.check_headers(headers, defines)

  def test_implementations(self):
    # Configuration supplies the platform; implementations declare everything else they use.
    for path in sorted((ROOT / "board").rglob("*.h")):
      header = path.relative_to(ROOT).as_posix()
      if header in INTERFACES or any(part in ("inc", "obj", "crypto") for part in path.parts) or path.name == "fake_stm.h":
        continue
      defines = []
      if "body" in path.parts:
        defines.append("PANDA_BODY")
      elif "jungle" in path.parts:
        defines.append("PANDA_JUNGLE")
      if path.name in ("bootstub.h", "flasher.h", "llflash.h"):
        defines.append("BOOTSTUB")
      with self.subTest(header=header):
        self.check_headers(["board/config.h", header], defines)


if __name__ == "__main__":
  unittest.main()
