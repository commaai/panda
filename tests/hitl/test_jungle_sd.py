#!/usr/bin/env python3
"""
HITL tests for jungle v2 SD card CAN replay USB commands (0xa5, 0xa6).

Requires a physical panda jungle v2. Skipped automatically when NO_JUNGLE=1
or when the jungle fixture is unavailable (see conftest.py).
"""
import struct
import unittest

import pytest

from panda import PandaJungle

SD_REPLAY_IDLE   = 0
SD_REPLAY_ACTIVE = 1
SD_REPLAY_DONE   = 2
SD_REPLAY_ERROR  = 3


class TestJungleSDReplay(unittest.TestCase):
  @pytest.fixture(autouse=True)
  def _attach_jungle(self, panda_jungle):
    self.jungle = panda_jungle

  # ------------------------------------------------------------------
  # 0xa6: status command
  # ------------------------------------------------------------------

  def test_sd_status_returns_dict(self):
    status = self.jungle.sd_replay_status()
    self.assertIn("state", status)
    self.assertIn("total_records", status)
    self.assertIn("current_record", status)

  def test_sd_status_state_in_range(self):
    status = self.jungle.sd_replay_status()
    self.assertIn(status["state"], (SD_REPLAY_IDLE, SD_REPLAY_ACTIVE, SD_REPLAY_DONE, SD_REPLAY_ERROR))

  def test_sd_status_raw_packet_size(self):
    """Raw 0xa6 response must be exactly 9 bytes parseable as <BII."""
    dat = self.jungle._handle.controlRead(PandaJungle.REQUEST_IN, 0xa6, 0, 0, 9)
    self.assertEqual(len(dat), 9)
    state, total, current = struct.unpack("<BII", dat)
    self.assertIn(state, (SD_REPLAY_IDLE, SD_REPLAY_ACTIVE, SD_REPLAY_DONE, SD_REPLAY_ERROR))
    self.assertGreaterEqual(total, 0)
    self.assertGreaterEqual(current, 0)

  def test_sd_status_no_valid_image(self):
    """Without a provisioned SD image, firmware should stay IDLE with 0 records."""
    status = self.jungle.sd_replay_status()
    # If no SD card or no valid image was written, sd_replay_init() leaves
    # total_records == 0 and state == IDLE.
    if status["total_records"] == 0:
      self.assertEqual(status["state"], SD_REPLAY_IDLE)
    # If a card IS present with a valid image, total_records > 0 is fine —
    # we just verify it's not claiming to be mid-replay without being started.
    else:
      self.assertNotEqual(status["state"], SD_REPLAY_ACTIVE)

  # ------------------------------------------------------------------
  # 0xa5: start / stop commands
  # ------------------------------------------------------------------

  def test_sd_stop_is_idempotent(self):
    """Calling stop twice must not corrupt state."""
    self.jungle.sd_replay_stop()
    self.jungle.sd_replay_stop()
    status = self.jungle.sd_replay_status()
    self.assertEqual(status["state"], SD_REPLAY_IDLE)

  def test_sd_start_without_image_does_not_hang_active(self):
    """Start with no valid image must not leave state stuck as ACTIVE."""
    status_before = self.jungle.sd_replay_status()
    if status_before["total_records"] != 0:
      pytest.skip("SD card has a valid image; skipping no-image start test")

    self.jungle.sd_replay_start()
    status = self.jungle.sd_replay_status()
    # sd_replay_start() checks total_records before activating;
    # with 0 records it should stay IDLE.
    self.assertNotEqual(status["state"], SD_REPLAY_ACTIVE)

    # Leave clean
    self.jungle.sd_replay_stop()

  def test_sd_stop_after_start(self):
    """Start then immediately stop should return to IDLE."""
    status_before = self.jungle.sd_replay_status()
    if status_before["total_records"] == 0:
      pytest.skip("No valid SD image; start would be a no-op")

    self.jungle.sd_replay_start()
    self.jungle.sd_replay_stop()
    status = self.jungle.sd_replay_status()
    self.assertEqual(status["state"], SD_REPLAY_IDLE)

  def test_sd_current_record_resets_on_start(self):
    """current_record must reset to 0 each time replay is started."""
    status_before = self.jungle.sd_replay_status()
    if status_before["total_records"] == 0:
      pytest.skip("No valid SD image loaded")

    self.jungle.sd_replay_start()
    status = self.jungle.sd_replay_status()
    self.assertEqual(status["current_record"], 0)

    self.jungle.sd_replay_stop()


if __name__ == "__main__":
  unittest.main()
