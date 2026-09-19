#!/usr/bin/env python3
"""Live microphone-test status and playback of saved Before/After comparisons."""
import argparse
import fcntl
import json
import textwrap
import time
import wave
from functools import partial
from pathlib import Path

import numpy as np
import pyray as rl
import sounddevice as sd

from openpilot.common.hardware import PC
from openpilot.system.micd import patch_sounddevice
from openpilot.system.ui.lib.application import gui_app
from openpilot.system.ui.widgets.button import Button


class ComparisonPlayer:
  def __init__(self):
    lock_path = Path('/tmp/comma-mic/audio.lock') if PC else Path('/data/mic-lab/audio.lock')
    lock_path.parent.mkdir(parents=True, exist_ok=True)
    self.audio_lock = lock_path.open('a')
    self.playing_until = 0
    self.label = ''

  def stop(self):
    sd.stop()
    fcntl.flock(self.audio_lock, fcntl.LOCK_UN)
    self.playing_until = 0
    self.label = ''

  def play(self, path, label):
    if self.playing_until:
      self.stop()
    try:
      fcntl.flock(self.audio_lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
      self.label = 'Recording in progress'
      return
    try:
      with wave.open(str(path), 'rb') as recording:
        rate = recording.getframerate()
        samples = np.frombuffer(recording.readframes(recording.getnframes()), dtype='<i2').reshape(-1, recording.getnchannels())
      sd.play(samples, rate)
      self.playing_until = time.monotonic() + len(samples) / rate
      self.label = label
    except Exception as error:
      self.stop()
      self.label = str(error)


def draw_text(text, x, y, width, size, max_lines=2):
  lines = textwrap.wrap(text, width=max(10, int(width / (size * 0.56))))[:max_lines]
  for line in lines:
    rl.draw_text_ex(gui_app.font(), line, rl.Vector2(x, y), size, 0, rl.WHITE)
    y += size * 1.25


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('status', type=Path)
  parser.add_argument('--comparisons', type=Path)
  parser.add_argument('--screenshot', type=Path)
  args = parser.parse_args()
  patch_sounddevice(sd)
  gui_app.init_window('comma mic lab', 10)
  rl.set_target_fps(10)
  width, height = gui_app.width, gui_app.height
  margin = width * 0.045
  size = min(width / 25, height / 15)
  player = ComparisonPlayer()
  comparisons = []
  manifest_changed = None
  page = 0

  def next_page():
    nonlocal page
    page = (page + 1) % max(1, (len(comparisons) + 1) // 2)

  next_button = Button('Next', next_page, font_size=int(size * 0.8), text_padding=0)
  stop = Button('Stop', player.stop, font_size=int(size * 0.8), text_padding=0)
  status = {}
  frames = 0
  screenshot_page = None
  try:
    for _ in gui_app.render():
      if args.comparisons:
        manifest = args.comparisons / 'manifest.json'
        changed = manifest.stat().st_mtime_ns
        if changed != manifest_changed:
          comparisons = []
          for entry in json.loads(manifest.read_text()):
            buttons = [Button(side.title(), partial(player.play, args.comparisons / entry[side], entry['title'] + ': ' + side),
                              font_size=int(size), text_padding=0) for side in ('before', 'after')]
            comparisons.append((entry, buttons))
          manifest_changed = changed
          page = min(page, max(0, (len(comparisons) - 1) // 2))
      try:
        status = json.loads(args.status.read_text())
      except (OSError, ValueError):
        pass
      allowed = status.get('allow_playback', False)
      if player.playing_until and (not allowed or time.monotonic() >= player.playing_until):
        player.stop()
      draw_text(status.get('title', 'Microphone lab'), margin, height * 0.04, width - 2 * margin, size * 1.1, 1)
      draw_text(status.get('message', ''), margin, height * 0.14, width - 2 * margin, size * 0.77)
      draw_text(status.get('detail', ''), margin, height * 0.28, width - 2 * margin, size * 0.57)
      if status.get('duration', 0) > 0:
        now = time.time()  # noqa: TID251 - Shared wall-clock timestamps across the PC and device.
        progress = min(1, max(0, (now - status['started']) / status['duration']))
        rl.draw_rectangle(int(margin), int(height * 0.385), int((width - 2 * margin) * progress), 4, rl.WHITE)
      for index, (entry, buttons) in enumerate(comparisons[page * 2:page * 2 + 2]):
        y = height * (0.43 + index * 0.21)
        draw_text(entry['title'], margin, y, width - 2 * margin, size * 0.8, 1)
        button_width = (width - margin * 3) / 2
        for column, button in enumerate(buttons):
          button.set_enabled(allowed)
          button.render(rl.Rectangle(margin + column * (button_width + margin), y + height * 0.06, button_width, height * 0.12))
      footer = player.label or ('Same gain per pair; offline repair is labeled' if allowed else 'Playback paused while measuring')
      footer = f'{page + 1}/{max(1, (len(comparisons) + 1) // 2)}  ' + footer
      draw_text(footer, margin, height * 0.88, width * 0.51, size * 0.5)
      next_button.set_enabled(len(comparisons) > 2)
      next_button.render(rl.Rectangle(width * 0.60, height * 0.87, width * 0.17, height * 0.1))
      stop.render(rl.Rectangle(width * 0.8, height * 0.87, width * 0.16, height * 0.1))
      frames += 1
      if args.screenshot and frames >= 3 and page != screenshot_page:
        rl.rl_draw_render_batch_active()
        screenshot = rl.load_image_from_screen()
        rl.export_image(screenshot, str(args.screenshot))
        rl.unload_image(screenshot)
        screenshot_page = page
  finally:
    player.stop()
    player.audio_lock.close()


if __name__ == '__main__':
  main()
