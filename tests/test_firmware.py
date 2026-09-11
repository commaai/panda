"""Check hardware-critical linkage in the firmware images built by SCons."""
import re
import struct
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
IMAGES = [ROOT / 'board/obj' / variant / f'{image}.elf'
          for variant in ('panda_h7', 'panda_jungle_h7', 'body_h7')
          for image in ('main', 'bootstub')]


def binutils(tool, *args):
  return subprocess.check_output([f'arm-none-eabi-{tool}', *map(str, args)], text=True)


class TestFirmware(unittest.TestCase):
  @classmethod
  def setUpClass(cls):
    cls.symbols = {}
    for image in IMAGES:
      if not image.is_file():
        raise RuntimeError(f'Build firmware with scons before running this test: {image}')
      cls.symbols[image] = {}
      for line in binutils('nm', '--format=posix', '-S', image).splitlines():
        fields = line.split()
        if len(fields) >= 3:
          cls.symbols[image][fields[0]] = (fields[1], int(fields[2], 16), int(fields[3], 16) if len(fields) > 3 else 0)

  def test_vectors_and_startup(self):
    startup = (ROOT / 'board/stm32h7/startup_stm32h7x5xx.s').read_text().split('g_pfnVectors:', 1)[1]
    vectors = re.findall(r'^\s*\.word\s+(\w+)', startup, re.MULTILINE)
    wrappers = [name for name in vectors if name.endswith('_IRQHandler')]
    for image in IMAGES:
      with self.subTest(image=image):
        symbols = self.symbols[image]
        with tempfile.TemporaryDirectory() as directory:
          raw = Path(directory) / 'vectors.bin'
          binutils('objcopy', f'--dump-section=.isr_vector={raw}', image, Path(directory) / 'image.elf')
          values = struct.unpack(f'<{len(vectors)}I', raw.read_bytes())
        for name, value in zip(vectors, values, strict=True):
          expected = 0 if name == '0' else symbols[name][1]
          if name.endswith(('_Handler', '_IRQHandler')):
            expected |= 1  # Cortex-M vectors must select Thumb instructions.
          self.assertEqual(value, expected, name)
        for name in wrappers:
          self.assertEqual(symbols[name][0], 'T', name)
          self.assertNotEqual(symbols[name][1], symbols['Default_Handler'][1], name)
        for name in ('early_initialization', '__initialize_hardware_early'):
          self.assertEqual(symbols[name][0], 'T', name)
        reset = binutils('objdump', '-d', '--disassemble=Reset_Handler', image)
        early = binutils('objdump', '-d', '--disassemble=__initialize_hardware_early', image)
        self.assertRegex(reset, r'bl\s+[^\n]*<__initialize_hardware_early>')
        self.assertRegex(early, r'b(?:\.w|l)\s+[^\n]*<early_initialization>')

  def test_dma_and_can_memory(self):
    for image in IMAGES:
      expected = {'spi_buf_rx': (0x30000000, 0x30008000), 'spi_buf_tx': (0x30000000, 0x30008000)}
      if image.stem == 'main':
        expected.update({'elems_rx_q': (0x24000000, 0x24050000),
                         'elems_tx1_q': (0, 0x10000), 'elems_tx2_q': (0, 0x10000),
                         'elems_tx3_q': (0x20000000, 0x20020000)})
      if image.parent.name == 'panda_h7':
        expected.update(dict.fromkeys(('sound_rx_buf', 'sound_tx_buf', 'mic_rx_buf', 'mic_tx_buf'), (0x38000000, 0x38004000)))
      for name, (start, end) in expected.items():
        with self.subTest(image=image, buffer=name):
          _, address, size = self.symbols[image][name]
          self.assertGreater(size, 0)
          self.assertGreaterEqual(address, start)
          self.assertLessEqual(address + size, end)
          self.assertEqual(address % 4, 0)

  def test_flash_boundaries(self):
    for image in IMAGES:
      with self.subTest(image=image):
        start, end = (0x08000000, 0x08020000) if image.stem == 'bootstub' else (0x08020000, 0x08100000)
        self.assertEqual(self.symbols[image]['g_pfnVectors'][1], start)
        segments = re.findall(r'^\s*LOAD\s+\S+\s+\S+\s+(0x\w+)\s+(0x\w+)',
                              binutils('readelf', '-W', '-l', image), re.MULTILINE)
        flash_segments = [(int(address, 16), int(size, 16)) for address, size in segments
                          if 0x08000000 <= int(address, 16) < 0x08100000 and int(size, 16)]
        self.assertTrue(flash_segments)
        for address, size in flash_segments:
          self.assertGreaterEqual(address, start)
          self.assertLessEqual(address + size, end)


if __name__ == '__main__':
  unittest.main()
