#!/usr/bin/env python3
"""Temporary on-device capture probe. Does not change mixer settings or parameters."""
import argparse
import contextlib
import fcntl
import json
import struct
import threading
import time
import wave
from pathlib import Path
from unittest.mock import patch

import numpy as np
import sounddevice as sd

from panda.python.spi import PandaSpiHandle
from openpilot.system import micd


class CapturePublisher:
  def __init__(self, delegate=None):
    self.delegate = delegate
    self.last_audio = b""

  def send(self, service, message):
    if service == "rawAudioData":
      self.last_audio = bytes(message.rawAudioData.data)
    if self.delegate is not None:
      self.delegate.send(service, message)


def write_wav(path, samples, sample_rate):
  with wave.open(str(path), "wb") as output:
    output.setnchannels(samples.shape[1])
    output.setsampwidth(2)
    output.setframerate(sample_rate)
    output.writeframes(samples.astype("<i2").tobytes())


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("output", type=Path)
  parser.add_argument("--seconds", type=float, default=20)
  parser.add_argument("--rate", type=int, default=micd.SAMPLE_RATE)
  parser.add_argument("--blocksize", type=int, default=micd.SAMPLE_BUFFER)
  parser.add_argument("--channels", type=int, default=1)
  parser.add_argument("--dtype", choices=["float32", "int16", "int32"], default="float32")
  parser.add_argument("--device", default=None)
  parser.add_argument("--latency", default="high")
  parser.add_argument("--workload", choices=["capture", "micd", "micd-ipc"], default="capture")
  parser.add_argument("--ipc-warmup", type=float, default=1.2, help="Seconds for logger subscribers to settle before micd-ipc capture")
  parser.add_argument("--stimulus", choices=["none", "tones", "steady-tone"], default="none")
  parser.add_argument("--no-playback", action="store_true", help="Disable silent playback clocking (normally needed on comma four)")
  parser.add_argument("--volume", type=float, default=0.03, help="Playback peak amplitude, 0 to 0.1")
  parser.add_argument("--replay", action="store_true", help="Replay the captured clip after saving, at a modest peak level")
  parser.add_argument("--panda-debug", action="store_true", help="Read temporary MICRESAMPLEDBG diagnostic counters")
  parser.add_argument("--replay-seconds", type=float, default=15, help="Maximum replay duration")
  args = parser.parse_args()
  if not args.output.resolve().is_relative_to("/data"):
    parser.error("device output must be under /data")
  if not 0 <= args.ipc_warmup <= 10:
    parser.error("ipc-warmup must be between 0 and 10 seconds")
  if args.seconds <= 0 or args.seconds > 300:
    parser.error("seconds must be between 0 and 300")
  if args.stimulus == "steady-tone" and args.seconds < 4:
    parser.error("steady-tone requires at least 4 seconds")
  if args.replay_seconds <= 0:
    parser.error("replay-seconds must be positive")
  if args.blocksize <= 0:
    parser.error("blocksize must be positive")
  if not 0 <= args.volume <= 0.1:
    parser.error("playback volume must be between 0 and 0.1")
  if args.workload.startswith("micd") and (args.rate != micd.SAMPLE_RATE or args.channels != 1 or args.dtype != "float32"):
    parser.error("micd workload requires its native rate, mono, and float32")

  audio_lock = open("/data/mic-lab/audio.lock", "a")
  fcntl.flock(audio_lock, fcntl.LOCK_EX)
  micd.patch_sounddevice(sd)
  with contextlib.closing(PandaSpiHandle()) as panda_handle:
    firmware_version = panda_handle.controlRead(0xc0, 0xd6, 0, 0, 64).decode()
    firmware_signature = (panda_handle.controlRead(0xc0, 0xd3, 0, 0, 64) +
                          panda_handle.controlRead(0xc0, 0xd4, 0, 0, 64)).hex()
  if args.panda_debug and "MICRESAMPLEDBG" not in firmware_version:
    parser.error("panda-debug requires the instrumented microphone experiment firmware")
  delegate = micd.messaging.PubMaster(["soundPressure", "rawAudioData"]) if args.workload == "micd-ipc" else None
  if delegate is not None:
    time.sleep(args.ipc_warmup)
    if not delegate.all_readers_updated("rawAudioData"):
      raise RuntimeError("No settled rawAudioData subscriber; start loggerd before capture")
  publisher = CapturePublisher(delegate)
  with patch.object(micd.messaging, "PubMaster", return_value=publisher):
    microphone = micd.Mic()

  target_frames = round(args.rate * args.seconds)
  samples = np.empty((target_frames, args.channels), dtype=args.dtype)
  logged_samples = np.empty(target_frames, dtype=np.int16)
  # Preallocate diagnostics so the normal callback only copies into existing arrays.
  records = np.zeros(((target_frames + args.blocksize - 1) // args.blocksize, 8), dtype=np.float64)
  done = threading.Event()
  frame_offset = 0
  callback_count = 0
  callback_error = []
  playback_rate = 48000
  playback = np.zeros(round(playback_rate * args.seconds), dtype=np.float32)
  if args.stimulus == "tones":
    for section, frequency in enumerate((500, 1000, 2000, 3000)):
      start = round((2 + section * 3) * playback_rate)
      length = min(2 * playback_rate, len(playback) - start)
      if length <= 0:
        break
      envelope = np.ones(length)
      ramp = min(playback_rate // 10, length // 2)
      envelope[:ramp] = np.linspace(0, 1, ramp)
      envelope[-ramp:] = np.linspace(1, 0, ramp)
      playback[start:start + length] = args.volume * envelope * np.sin(2 * np.pi * frequency * np.arange(length) / playback_rate)
  elif args.stimulus == "steady-tone":
    start = playback_rate
    length = len(playback) - 2 * playback_rate
    envelope = np.ones(length)
    ramp = playback_rate // 10
    envelope[:ramp] = np.linspace(0, 1, ramp)
    envelope[-ramp:] = np.linspace(1, 0, ramp)
    playback[start:start + length] = args.volume * envelope * np.sin(2 * np.pi * 1000 * np.arange(length) / playback_rate)
  playback_offset = 0
  playback_underflows = 0

  def output_callback(outdata, frames, timing, status):
    nonlocal playback_offset, playback_underflows
    playback_underflows += int(status.output_underflow)
    available = min(frames, len(playback) - playback_offset)
    outdata.fill(0)
    outdata[:available, 0] = playback[playback_offset:playback_offset + available]
    playback_offset += available


  def callback(indata, frames, timing, status):
    nonlocal frame_offset, callback_count
    started = time.monotonic()
    try:
      copied_frames = min(frames, target_frames - frame_offset)
      samples[frame_offset:frame_offset + copied_frames] = indata[:copied_frames]
      if args.workload.startswith("micd"):
        microphone.callback(indata, frames, timing, status)
        encoded = np.frombuffer(publisher.last_audio, dtype=np.int16)
        if len(encoded) != frames:
          raise RuntimeError(f"micd published {len(encoded)} samples for {frames} input frames")
        logged_samples[frame_offset:frame_offset + copied_frames] = encoded[:copied_frames]
      records[callback_count] = (frame_offset, frames, started, timing.inputBufferAdcTime,
                                 timing.currentTime, int(status.input_overflow), int(status.input_underflow),
                                 time.monotonic() - started)
      frame_offset += copied_frames
      callback_count += 1
    except Exception as error:
      callback_error.append(repr(error))
      done.set()
      raise sd.CallbackAbort from error
    if frame_offset >= target_frames:
      done.set()
      raise sd.CallbackStop

  device = int(args.device) if args.device and args.device.isdecimal() else args.device
  latency = args.latency if args.latency in ("high", "low") else float(args.latency)
  with sd.InputStream(device=device, channels=args.channels, samplerate=args.rate, dtype=args.dtype,
                      blocksize=args.blocksize, latency=latency, callback=callback) as stream:
    stream_info = {"device": stream.device, "dtype": stream.dtype, "samplerate": stream.samplerate,
                   "latency": stream.latency, "blocksize": stream.blocksize,
                   "device_info": dict(sd.query_devices(stream.device)),
                   "portaudio": sd.get_portaudio_version()}
    stream_info["alsa_capture_hw_params"] = Path("/proc/asound/card0/pcm0c/sub0/hw_params").read_text()
    print(json.dumps({"recording": True, **stream_info}), flush=True)
    output_stream = contextlib.nullcontext()
    if not args.no_playback:
      output_stream = sd.OutputStream(channels=1, samplerate=playback_rate, dtype="float32", callback=output_callback)
    with output_stream:
      completed = done.wait(args.seconds + 15)
      stream_info["cpu_load"] = stream.cpu_load

  if callback_error or not completed:
    raise RuntimeError(f"Capture failed: {callback_error or 'timeout'}")
  samples = samples[:frame_offset]
  records = records[:callback_count]
  args.output.parent.mkdir(parents=True, exist_ok=True)
  np.savez(args.output.with_suffix(".npz"), samples=samples, callbacks=records,
           stimulus=playback if args.stimulus != "none" else np.empty(0, dtype=np.float32),
           logged_samples=logged_samples[:frame_offset] if args.workload.startswith("micd") else np.empty(0, dtype=np.int16))
  if args.workload.startswith("micd"):
    pcm = logged_samples[:frame_offset, None]
  elif args.dtype == "float32":
    # Deliberately match micd conversion; native floats are retained in the NPZ.
    pcm = (samples * 32767).astype(np.int16)
  elif args.dtype == "int32":
    pcm = (samples >> 16).astype(np.int16)
  else:
    pcm = samples
  metadata = {**vars(args), "output": str(args.output), "stream": stream_info,
              "frames": frame_offset, "callbacks": callback_count, "micd_source": micd.__file__,
              "playback_rate": playback_rate, "playback_underflows": playback_underflows,
              "panda_version": firmware_version, "panda_signature": firmware_signature,
              "callback_columns": ["frame_offset", "frames", "monotonic", "adc_time", "current_time",
                                   "input_overflow", "input_underflow", "duration"]}
  if args.panda_debug:
    with contextlib.closing(PandaSpiHandle()) as panda_handle:
      values = struct.unpack("<8I", panda_handle.controlRead(0xc0, 0xef, 0, 0, 32))
    q20_versions = ("MICRESAMPLEDBG3", "MICRESAMPLEDBG4", "MICRESAMPLEDBG5", "MICRESAMPLEDBG6", "MICRESAMPLEDBG7")
    phase_bits = 20 if any(tag in firmware_version for tag in q20_versions) else 16
    metadata["panda_mic_diagnostics"] = dict(zip(
      ["output_blocks", "resyncs", "min_available", "max_available", f"min_step_q{phase_bits}",
       f"max_step_q{phase_bits}", "max_irq_us", "reserved"], values, strict=True))
  args.output.with_suffix(".json").write_text(json.dumps(metadata, indent=2) + "\n")
  write_wav(args.output.with_suffix(".wav"), pcm, args.rate)
  print(json.dumps({"saved": str(args.output), "frames": frame_offset,
                    "overflows": int(records[:, 5].sum()), "underflows": int(records[:, 6].sum())}), flush=True)
  if args.replay:
    replay = pcm[:round(args.replay_seconds * args.rate)].astype(np.float32) / 32768
    peak = float(np.max(np.abs(replay)))
    gain = min(4.0, args.volume / max(peak, 1e-8))
    print(json.dumps({"replaying": True, "gain": gain, "peak": peak * gain}), flush=True)
    sd.play(replay * gain, args.rate)
    sd.wait()


if __name__ == "__main__":
  main()
