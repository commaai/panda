import unittest
from types import SimpleNamespace
from unittest.mock import Mock, call

from panda import Panda


class TestDebugRead(unittest.TestCase):
  def test_empty_and_partial_reads(self):
    handle = Mock()
    handle.controlRead.side_effect = [b'first', b' second', b'']
    self.assertEqual(Panda.debug_read(SimpleNamespace(_handle=handle)), b'first second')
    self.assertEqual(handle.controlRead.call_args_list, [call(Panda.REQUEST_IN, 0xb6, 0, 0, 64)] * 3)

  def test_limit_does_not_drain_extra_bytes(self):
    handle = Mock()
    handle.controlRead.side_effect = [b'a' * 64, b'b' * 6]
    panda = SimpleNamespace(_handle=handle)
    self.assertEqual(Panda.debug_read(panda, maxlen=70), b'a' * 64 + b'b' * 6)
    self.assertEqual(handle.controlRead.call_args_list, [
      call(Panda.REQUEST_IN, 0xb6, 0, 0, 64),
      call(Panda.REQUEST_IN, 0xb6, 0, 0, 6),
    ])
    handle.reset_mock()
    self.assertEqual(Panda.debug_read(panda, maxlen=0), b'')
    handle.controlRead.assert_not_called()
