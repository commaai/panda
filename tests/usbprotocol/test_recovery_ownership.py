import unittest
from unittest.mock import Mock, patch

from panda import McuType, Panda, PandaDFU
from panda.python.spi import PandaSpiException


class TestRecoveryOwnership(unittest.TestCase):
  def test_recovery_transfers_ownership_and_closes_failures(self):
    for reset in (False, True):
      for failure in (None, 'reset', 'wait', 'dfu', 'connect', 'flash'):
        if failure == 'reset' and not reset:
          continue
        with self.subTest(reset=reset, failure=failure):
          owner = ['panda']
          panda = Panda.__new__(Panda)
          panda.get_dfu_serial = Mock(return_value='SERIAL')

          def close():
            self.assertNotEqual(owner[0], 'dfu')
            owner[0] = None

          def reset_panda(**kwargs):
            if failure == 'reset':
              raise OSError('reset failed')
            # Entering the bootstub reconnects; entering ROM DFU closes Panda.
            owner[0] = None if kwargs.get('enter_bootloader') else 'panda'

          def wait(serial, timeout):
            self.assertIsNone(owner[0], 'DFU discovery started while Panda still owns SPI')
            return failure != 'wait'

          def connect(*args):
            self.assertIsNone(owner[0], 'DFU still owns SPI when reconnect starts')
            owner[0] = 'panda'
            if failure == 'connect':
              raise OSError('connect failed')

          def flash():
            self.assertEqual(owner[0], 'panda')
            if failure == 'flash':
              raise OSError('flash failed')

          class DFU:
            def __init__(dfu_self, serial):
              self.assertIsNone(owner[0])
              self.assertEqual(serial, 'SERIAL')
              owner[0] = 'dfu'

            def __enter__(dfu_self):
              return dfu_self

            def __exit__(dfu_self, *args):
              owner[0] = None

            def recover(dfu_self):
              self.assertEqual(owner[0], 'dfu')
              if failure == 'dfu':
                raise OSError('DFU recovery failed')

          panda.close = Mock(side_effect=close)
          panda.reset = Mock(side_effect=reset_panda)
          panda.wait_for_dfu = Mock(side_effect=wait)
          panda.connect = Mock(side_effect=connect)
          panda.flash = Mock(side_effect=flash)
          with patch('panda.python.PandaDFU', DFU):
            if failure not in (None, 'wait'):
              with self.assertRaises(OSError):
                panda.recover(reset=reset)
            else:
              self.assertEqual(panda.recover(reset=reset), failure is None)
          self.assertEqual(owner[0], 'panda' if failure is None else None)
          if reset and failure != 'reset':
            self.assertEqual(panda.reset.call_args_list[0].kwargs, {'enter_bootstub': True})
            self.assertEqual(panda.reset.call_args_list[1].kwargs, {'enter_bootloader': True})
          elif not reset:
            panda.reset.assert_not_called()

  def test_failed_constructor_closes_open_connection(self):
    handle = Mock()

    def connect(panda, claim):
      panda._handle = handle
      panda._handle_open = True
      panda._context = None
      raise OSError('initial configuration failed')

    with patch.object(Panda, 'connect', connect):
      with self.assertRaises(OSError):
        Panda('SERIAL')
    handle.close.assert_called_once()

  def test_discovery_io_error_closes_handle(self):
    handle = Mock()
    handle.get_protocol_version.side_effect = OSError('SPI ioctl failed')
    with patch('panda.python.PandaSpiHandle', return_value=handle):
      with self.assertRaises(OSError):
        Panda.spi_connect(None)
    handle.close.assert_called_once()

  def test_dfu_listing_releases_retained_probe_before_reopen(self):
    owner = [None]
    handles = []
    uid = bytes(range(12)).hex()
    serial = PandaDFU.st_serial_to_dfu_serial(uid, McuType.H7)

    def open_spi():
      self.assertIsNone(owner[0], 'previous DFU probe still owns SPI')
      handle = Mock()
      handle.get_uid.return_value = uid
      handle.get_mcu_type.return_value = McuType.H7
      handle.close.side_effect = lambda: owner.__setitem__(0, None)
      owner[0] = handle
      handles.append(handle)  # Keep probes alive, as a retained retry traceback can.
      return handle

    with patch('panda.python.dfu.STBootloaderSPIHandle', side_effect=open_spi):
      self.assertEqual(PandaDFU.spi_list(), [serial])
      handles[0].close.assert_called_once()
      with patch.object(PandaDFU, 'usb_connect', return_value=(None, None)):
        with PandaDFU(serial):
          self.assertIs(owner[0], handles[1])
      self.assertIsNone(owner[0])
      handles[1].close.assert_called_once()

  def test_dfu_probe_errors_and_mismatch_close_handle(self):
    for error in (PandaSpiException('protocol error'), OSError('ioctl error'), None):
      with self.subTest(error=error):
        handle = Mock()
        handle.get_uid.side_effect = error
        handle.get_uid.return_value = bytes(range(12)).hex()
        handle.get_mcu_type.return_value = McuType.H7
        with patch('panda.python.dfu.STBootloaderSPIHandle', return_value=handle):
          if isinstance(error, OSError):
            with self.assertRaises(OSError):
              PandaDFU.spi_connect('different serial')
          else:
            self.assertEqual(PandaDFU.spi_connect('different serial'), (None, None))
        handle.close.assert_called_once()

  def test_dfu_listing_error_closes_probe(self):
    handle = Mock()
    handle.get_uid.side_effect = PandaSpiException('UID read failed')
    with patch.object(PandaDFU, 'spi_connect', return_value=(None, handle)):
      self.assertEqual(PandaDFU.spi_list(), [])
    handle.close.assert_called_once()

  def test_dfu_constructor_error_closes_handle_and_context(self):
    for usb in (False, True):
      with self.subTest(usb=usb):
        handle, context = Mock(), Mock()
        handle.get_mcu_type.side_effect = OSError('MCU type failed')
        with (patch.object(PandaDFU, 'usb_connect', return_value=(context, handle if usb else None)),
              patch.object(PandaDFU, 'spi_connect', return_value=(None, handle))):
          with self.assertRaises(OSError):
            PandaDFU(None)
        handle.close.assert_called_once()
        context.close.assert_called_once()
