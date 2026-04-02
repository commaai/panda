#!/usr/bin/env python3
"""
HITL tests for jungle v2 SD card CAN replay USB commands (0xa5, 0xa6).

Requires a physical panda jungle v2. Skipped automatically when NO_JUNGLE=1
or when the jungle fixture is unavailable (see conftest.py).
"""
import struct

import pytest

from panda import PandaJungle

SD_REPLAY_IDLE   = 0
SD_REPLAY_ACTIVE = 1
SD_REPLAY_DONE   = 2
SD_REPLAY_ERROR  = 3


# ------------------------------------------------------------------
# 0xa6: status command
# ------------------------------------------------------------------

def test_sd_status_returns_dict(panda_jungle):
  status = panda_jungle.sd_replay_status()
  assert "state" in status
  assert "total_records" in status
  assert "current_record" in status

def test_sd_status_state_in_range(panda_jungle):
  status = panda_jungle.sd_replay_status()
  assert status["state"] in (SD_REPLAY_IDLE, SD_REPLAY_ACTIVE, SD_REPLAY_DONE, SD_REPLAY_ERROR)

def test_sd_status_raw_packet_size(panda_jungle):
  """Raw 0xa6 response must be exactly 9 bytes parseable as <BII."""
  dat = panda_jungle._handle.controlRead(PandaJungle.REQUEST_IN, 0xa6, 0, 0, 9)
  assert len(dat) == 9
  state, total, current = struct.unpack("<BII", dat)
  assert state in (SD_REPLAY_IDLE, SD_REPLAY_ACTIVE, SD_REPLAY_DONE, SD_REPLAY_ERROR)
  assert total >= 0
  assert current >= 0

def test_sd_status_no_valid_image(panda_jungle):
  """Without a provisioned SD image, firmware should stay IDLE with 0 records."""
  status = panda_jungle.sd_replay_status()
  # If no SD card or no valid image was written, sd_replay_init() leaves
  # total_records == 0 and state == IDLE.
  if status["total_records"] == 0:
    assert status["state"] == SD_REPLAY_IDLE
  # If a card IS present with a valid image, total_records > 0 is fine —
  # we just verify it's not claiming to be mid-replay without being started.
  else:
    assert status["state"] != SD_REPLAY_ACTIVE


# ------------------------------------------------------------------
# 0xa5: start / stop commands
# ------------------------------------------------------------------

def test_sd_stop_is_idempotent(panda_jungle):
  """Calling stop twice must not corrupt state."""
  panda_jungle.sd_replay_stop()
  panda_jungle.sd_replay_stop()
  status = panda_jungle.sd_replay_status()
  assert status["state"] == SD_REPLAY_IDLE

def test_sd_start_without_image_does_not_hang_active(panda_jungle):
  """Start with no valid image must not leave state stuck as ACTIVE."""
  status_before = panda_jungle.sd_replay_status()
  if status_before["total_records"] != 0:
    pytest.skip("SD card has a valid image; skipping no-image start test")

  panda_jungle.sd_replay_start()
  status = panda_jungle.sd_replay_status()
  # sd_replay_start() checks total_records before activating;
  # with 0 records it should stay IDLE.
  assert status["state"] != SD_REPLAY_ACTIVE

  # Leave clean
  panda_jungle.sd_replay_stop()

def test_sd_stop_after_start(panda_jungle):
  """Start then immediately stop should return to IDLE."""
  status_before = panda_jungle.sd_replay_status()
  if status_before["total_records"] == 0:
    pytest.skip("No valid SD image; start would be a no-op")

  panda_jungle.sd_replay_start()
  panda_jungle.sd_replay_stop()
  status = panda_jungle.sd_replay_status()
  assert status["state"] == SD_REPLAY_IDLE

def test_sd_current_record_resets_on_start(panda_jungle):
  """current_record must reset to 0 each time replay is started."""
  status_before = panda_jungle.sd_replay_status()
  if status_before["total_records"] == 0:
    pytest.skip("No valid SD image loaded")

  panda_jungle.sd_replay_start()
  status = panda_jungle.sd_replay_status()
  assert status["current_record"] == 0

  panda_jungle.sd_replay_stop()


if __name__ == "__main__":
  pytest.main([__file__, "-v"])
