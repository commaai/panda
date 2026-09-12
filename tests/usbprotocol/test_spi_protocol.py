import ctypes
import struct
import subprocess
import tempfile
import unittest
import zlib
from pathlib import Path

import opendbc

FIXTURE = r'''
#define can_tx_comms_resume_spi unused_spi_resume
#include "tests/libpanda/panda.c"
#undef can_tx_comms_resume_spi
#include "board/crc.h"
static uint8_t uid[12];
#define UID_BASE uid
static uint32_t calls;
static uint8_t *rx;
static const uint8_t *tx;
static uint16_t tx_len;
int comms_control_handler(ControlPacket_t *req, uint8_t *response) {
  (void)req; response[0] = ++calls; return 1;
}
void comms_endpoint2_write(const uint8_t *data, uint32_t len) {
  (void)data; (void)len; calls++;
}
void llspi_init(void) {}
void llspi_mosi_dma(uint8_t *addr, int len) { rx = addr; (void)len; tx_len = 0; }
void llspi_miso_dma(const uint8_t *addr, int len) { tx = addr; tx_len = len; }
#ifdef TEST_BOOTSTUB
#define BOOTSTUB
#endif
// Mach-O needs both a segment and section; keep all other firmware attributes.
#ifdef __APPLE__
#define section(name) section("__DATA," name)
#endif
#include "board/drivers/spi.h"
#ifdef __APPLE__
#undef section
#endif
void test_reset(void) { calls = 0; spi_error_count = 0; spi_init(); }
uint32_t test_calls(void) { return calls; }
uint16_t test_frame(const uint8_t *data, uint16_t len, bool valid, uint8_t *out) {
#ifdef TEST_BOOTSTUB
  (void)valid;
  if (tx_len != 0) { spi_tx_done(false); return 0; }
  memcpy(rx, data, len); spi_rx_done();
#else
  memcpy(rx, data, len); spi_cs_end(len, valid);
#endif
  if (tx_len != 0) memcpy(out, tx, tx_len);
  return tx_len;
}
'''


class TestSpiProtocol(unittest.TestCase):
  @classmethod
  def setUpClass(cls):
    cls.build = tempfile.TemporaryDirectory()
    cls.addClassCleanup(cls.build.cleanup)
    repo = Path(__file__).resolve().parents[2]
    source = Path(cls.build.name) / 'fixture.c'
    source.write_text(FIXTURE)
    cls.drivers = []
    for bootstub in (False, True):
      library = Path(cls.build.name) / f'spi{int(bootstub)}.so'
      subprocess.run(['gcc', '-shared', '-fPIC', '-fno-builtin', '-std=gnu11', '-Wno-pointer-to-int-cast',
                      '-I' + str(repo), '-I' + str(repo / 'board'), '-I' + opendbc.INCLUDE_PATH,
                      *(['-DTEST_BOOTSTUB'] if bootstub else []), str(source), '-o', str(library)], check=True)
      driver = ctypes.CDLL(str(library))
      driver.test_frame.argtypes = [ctypes.c_char_p, ctypes.c_uint16, ctypes.c_bool, ctypes.c_void_p]
      driver.test_frame.restype = ctypes.c_uint16
      cls.drivers.append(driver)

  def setUp(self):
    for driver in self.drivers:
      driver.test_reset()
    self.driver = self.drivers[0]

  def frame(self, data=b'', valid=True):
    out = ctypes.create_string_buffer(4096)
    count = self.driver.test_frame(data, len(data), valid, out)
    return out.raw[:count]

  @staticmethod
  def request(endpoint, sequence, payload=b'', capacity=0, session=123):
    data = struct.pack('<BBHIQHBB', 0x5a, endpoint, len(payload), sequence, session, capacity, 3, 0) + payload
    return data + struct.pack('<I', zlib.crc32(data))

  def exchange(self, request):
    reply = self.frame(request)
    self.assertEqual(zlib.crc32(reply[:-4]), struct.unpack('<I', reply[-4:])[0])
    self.assertEqual(self.frame(), b'')
    return reply

  def init(self):
    self.assertEqual(self.exchange(self.request(0xfe, 0))[1], 0)

  def test_replay_identity_and_init_preserve_cache(self):
    self.init()
    request = self.request(0, 1, bytes(7), 1)
    reply = self.exchange(request)
    self.assertEqual(reply[1], 0)
    self.assertEqual(self.driver.test_calls(), 1)
    self.init()
    self.assertEqual(self.exchange(self.request(0, 1, b'X' * 7, 1))[1], 3)
    self.assertEqual(self.exchange(self.request(0, 1, bytes(7), 1, session=456))[1], 2)
    self.assertEqual(self.exchange(request), reply)
    self.assertEqual(self.driver.test_calls(), 1)
    self.assertEqual(self.exchange(self.request(0, 2, bytes(7), 1))[1], 0)
    self.assertEqual(self.driver.test_calls(), 2)

  def test_idle_and_malformed_boundaries_preserve_session(self):
    self.init()
    errors = ctypes.c_uint16.in_dll(self.driver, 'spi_error_count')
    for _ in range(12):
      self.assertEqual(self.frame(), b'')
    self.assertEqual(errors.value, 0)
    for data, valid in ((bytes(1), True), (bytes(23), True), (b'', False), (self.request(0, 1, bytes(7))[:-1], True)):
      before = errors.value
      self.assertEqual(self.frame(data, valid), b'')
      self.assertEqual(errors.value, before + 1)
    self.assertEqual(self.exchange(self.request(0, 1, bytes(7), 1))[1], 0)

  def test_aborted_read_retries_without_execution(self):
    self.init()
    request = self.request(0, 1, bytes(7), 1)
    reply = self.frame(request)
    self.assertEqual(self.frame(bytes(1)), b'')
    self.assertEqual(self.exchange(request), reply)
    self.assertEqual(self.driver.test_calls(), 1)

  def test_rejected_operation_advances_sequence(self):
    self.init()
    rejected = self.request(0xff, 1)
    self.assertEqual(self.exchange(rejected)[1], 1)
    self.assertEqual(self.exchange(rejected)[1], 1)
    self.assertEqual(self.driver.test_calls(), 0)
    self.assertEqual(self.exchange(self.request(0, 2, bytes(7), 1))[1], 0)

  def test_bootstub_discovery_control_flash_and_bounds(self):
    self.driver = self.drivers[1]
    version = self.frame(b'VERSION')
    self.assertEqual((len(version), version[23]), (25, 2))
    self.frame()
    for endpoint, payload in ((0, bytes(7)), (2, bytes(32))):
      header = struct.pack('<BBHH', 0x5a, endpoint, len(payload), 64)
      checksum = 0xab
      for value in header:
        checksum ^= value
      self.assertEqual(self.frame(header + bytes([checksum])), b'\x79')
      self.frame()
      checksum = 0xab
      for value in payload:
        checksum ^= value
      self.assertEqual(self.frame(payload + bytes([checksum]))[0], 0x85)
      self.frame()
    self.assertEqual(self.driver.test_calls(), 2)
    header = struct.pack('<BBHH', 0x5a, 2, 4096, 0)
    checksum = 0xab
    for value in header:
      checksum ^= value
    self.assertEqual(self.frame(header + bytes([checksum])), b'\x1f')
