#!/usr/bin/env python3
"""Capture from a comma over SSH and retain WAV, native samples, timings, and plots."""
import argparse
import json
import shlex
import subprocess
import uuid
import wave
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def repeated_runs(samples, delay, minimum=32):
  identical = np.all(samples[delay:] == samples[:-delay], axis=1)
  transitions = np.diff(np.r_[False, identical, False].astype(np.int8))
  starts = np.flatnonzero(transitions == 1)
  ends = np.flatnonzero(transitions == -1)
  runs = []
  for start, end in zip(starts, ends, strict=True):
    if end - start >= minimum and np.std(samples[start + delay:end + delay]) > 1e-7:
      runs.append({"start_sample": int(start + delay), "length": int(end - start), "delay": delay})
  return runs


def amplitude_db(value):
  return float(20 * np.log10(max(float(value), 1e-15)))


def tone_metrics(samples, rate, frequency, start, end, reference_peak):
  segment = samples[round(start * rate):round(end * rate), 0]
  if len(segment) < rate // 2:
    return None
  segment = segment - np.mean(segment)
  sample_times = np.arange(len(segment)) / rate

  def fit(candidate):
    phase = 2 * np.pi * candidate * sample_times
    basis = np.column_stack((np.sin(phase), np.cos(phase), np.ones(len(segment))))
    coefficients = np.linalg.lstsq(basis, segment, rcond=None)[0]
    residual = segment - basis @ coefficients
    return float(np.mean(residual ** 2)), coefficients, residual

  # Locate the peak near the known stimulus, then refine frequency to avoid counting
  # ordinary sample-clock error as distortion of a nominally fixed-frequency tone.
  transform_size = 2 ** int(np.ceil(np.log2(len(segment) * 4)))
  spectrum = np.abs(np.fft.rfft(segment * np.hanning(len(segment)), n=transform_size))
  frequencies = np.fft.rfftfreq(transform_size, 1 / rate)
  candidates = np.flatnonzero(np.abs(frequencies - frequency) < max(5, frequency * 0.005))
  peak_index = candidates[np.argmax(spectrum[candidates])]
  left = frequencies[peak_index] - rate / transform_size
  right = frequencies[peak_index] + rate / transform_size
  for _ in range(24):
    lower = left + (right - left) / 3
    upper = right - (right - left) / 3
    if fit(lower)[0] < fit(upper)[0]:
      right = upper
    else:
      left = lower
  measured_frequency = (left + right) / 2
  residual_power, coefficients, residual = fit(measured_frequency)
  fundamental_rms = float(np.hypot(*coefficients[:2]) / np.sqrt(2))
  harmonic_levels = {}
  for harmonic in range(2, min(6, int((rate / 2 - 1) / measured_frequency)) + 1):
    phase = 2 * np.pi * measured_frequency * harmonic * sample_times
    basis = np.column_stack((np.sin(phase), np.cos(phase)))
    harmonic_coefficients = np.linalg.lstsq(basis, residual, rcond=None)[0]
    harmonic_levels[str(harmonic)] = float(np.hypot(*harmonic_coefficients) / np.sqrt(2))
  harmonic_rms = np.sqrt(sum(value ** 2 for value in harmonic_levels.values()))
  noise_start, noise_end = max(0, start - 1.2), max(0, start - 0.7)
  noise = samples[round(noise_start * rate):round(noise_end * rate), 0]
  noise_rms = float(np.std(noise)) if len(noise) else 0
  apparent_snr = amplitude_db(fundamental_rms / max(noise_rms, 1e-15))
  return {
    "nominal_hz": frequency, "measured_hz": measured_frequency,
    "window_seconds": [start, end], "fundamental_dbfs": amplitude_db(fundamental_rms),
    "preceding_room_noise_dbfs": amplitude_db(noise_rms), "apparent_snr_db": apparent_snr,
    "tone_above_room_noise": apparent_snr >= 10,
    "path_gain_db": amplitude_db(fundamental_rms / max(reference_peak / np.sqrt(2), 1e-15)),
    "thd_harmonics_2_through_6_percent": float(100 * harmonic_rms / max(fundamental_rms, 1e-15)),
    "thdn_percent": float(100 * np.sqrt(residual_power) / max(fundamental_rms, 1e-15)),
    "harmonics_dbc": {key: amplitude_db(value / max(fundamental_rms, 1e-15)) for key, value in harmonic_levels.items()},
  }


def analyze(prefix):
  metadata = json.loads(prefix.with_suffix(".json").read_text())
  capture = np.load(prefix.with_suffix(".npz"))
  native = capture["samples"]
  records = capture["callbacks"]
  scale = 1 if native.dtype.kind == "f" else 2 ** (native.dtype.itemsize * 8 - 1)
  samples = native.astype(np.float64) / scale
  rate = metadata["rate"]
  with wave.open(str(prefix.with_suffix(".wav")), "rb") as recording:
    pcm = np.frombuffer(recording.readframes(recording.getnframes()), dtype="<i2").reshape(-1, recording.getnchannels())
  boundary_indices = records[1:, 0].astype(int)
  differences = np.abs(np.diff(samples, axis=0))
  # Abrupt jumps are candidates, not a diagnosis: real acoustic impulses also qualify.
  jump_indices = np.flatnonzero(np.max(differences, axis=1) > 0.1) + 1
  largest = np.argsort(np.max(differences, axis=1))[-10:][::-1] + 1
  rms = np.sqrt(np.mean(samples ** 2, axis=0))
  centered = samples - np.mean(samples, axis=0)
  ac_rms = np.sqrt(np.mean(centered ** 2, axis=0))
  report = {
    "capture": str(prefix), "seconds": len(samples) / rate,
    "peak": np.max(np.abs(samples), axis=0).tolist(),
    "stddev": np.std(samples, axis=0).tolist(),
    "dc_offset": np.mean(samples, axis=0).tolist(),
    "ac_rms_dbfs": [amplitude_db(value) for value in ac_rms],
    "crest_factor_db": [amplitude_db(peak / max(level, 1e-15)) for peak, level in
                        zip(np.max(np.abs(centered), axis=0), ac_rms, strict=True)],
    "constant_after_first_second": bool(np.all(samples[rate:] == samples[-1])) if len(samples) > rate else None,
    "rms_dbfs": (20 * np.log10(np.maximum(rms, 1e-15))).tolist(),
    "native_out_of_range_samples": int(np.count_nonzero(np.abs(samples) > 1)),
    "near_full_scale_samples": np.sum(np.abs(samples) >= 0.999, axis=0).tolist(),
    "pcm_near_rails_samples": np.sum(np.abs(pcm.astype(np.int32)) >= 32760, axis=0).tolist(),
    "input_overflow_callbacks": int(records[:, 5].sum()),
    "input_underflow_callbacks": int(records[:, 6].sum()),
    "max_callback_ms": float(np.max(records[:, 7]) * 1000),
    "callback_interval_ms_percentiles": np.percentile(np.diff(records[:, 2]) * 1000, [0, 50, 95, 99, 100]).tolist(),
    "adc_gap_ms_percentiles": np.percentile((np.diff(records[:, 3]) - records[:-1, 1] / rate) * 1000, [0, 50, 95, 99, 100]).tolist(),
    "jumps_over_0_1_full_scale": len(jump_indices),
    "jumps_at_callback_boundaries": int(np.isin(jump_indices, boundary_indices).sum()),
    "largest_jump_times": (largest / rate).tolist(),
    "largest_jump_sizes": np.max(differences[largest - 1], axis=1).tolist(),
    "exact_repeated_runs": {str(delay): repeated_runs(samples, delay) for delay in (512, 1024)} if rate == 48000 else {},
  }
  startup = samples[:min(len(samples), rate // 10)]
  report["first_100_ms"] = {
    "peak": np.max(np.abs(startup), axis=0).tolist(),
    "max_sample_jump": float(np.max(np.abs(np.diff(startup, axis=0)))),
  }
  if "panda_mic_diagnostics" in metadata:
    report["panda_mic_diagnostics"] = metadata["panda_mic_diagnostics"]
  report["quality_scope"] = "Uncalibrated speaker-room-microphone path; room noise contributes to distortion-plus-noise."
  if len(samples) > rate:
    steady = samples[rate:]
    report["after_first_second"] = {
      "peak": np.max(np.abs(steady), axis=0).tolist(),
      "rms_dbfs": [amplitude_db(value) for value in np.sqrt(np.mean(steady ** 2, axis=0))],
      "near_full_scale_samples": np.sum(np.abs(steady) >= 0.999, axis=0).tolist(),
      "max_sample_jump": float(np.max(np.abs(np.diff(steady, axis=0)))),
    }
  if metadata.get("stimulus") == "tones":
    report["tones"] = [tone_metrics(samples, rate, frequency, 2.4 + section * 3, 3.6 + section * 3, metadata["volume"])
                       for section, frequency in enumerate((500, 1000, 2000, 3000)) if (3.6 + section * 3) * rate < len(samples)]
  prefix.with_name(prefix.name + "_analysis.json").write_text(json.dumps(report, indent=2) + "\n")

  figure, axes = plt.subplots(4, 1, figsize=(14, 12), constrained_layout=True)
  block = max(1, rate // 100)
  complete = len(samples) // block * block
  envelope = samples[:complete].reshape(-1, block, samples.shape[1])
  envelope_time = np.arange(len(envelope)) * block / rate
  for channel in range(samples.shape[1]):
    axes[0].plot(envelope_time, envelope[:, :, channel].min(axis=1), label=f"channel {channel + 1} min")
    axes[0].plot(envelope_time, envelope[:, :, channel].max(axis=1), label=f"channel {channel + 1} max")
  axes[0].set(title=prefix.name + " — native capture", xlabel="seconds", ylabel="full scale")
  axes[0].legend()
  with np.errstate(divide="ignore"):
    axes[1].specgram(samples[:, 0], NFFT=1024, Fs=rate, noverlap=768)
  axes[1].set(xlabel="seconds", ylabel="Hz")
  center = int(largest[0])
  start, end = max(0, center - rate // 100), min(len(samples), center + rate // 100)
  axes[2].plot(np.arange(start, end) / rate, samples[start:end])
  for boundary in boundary_indices[(boundary_indices >= start) & (boundary_indices < end)]:
    axes[2].axvline(boundary / rate)
  axes[2].set(title="Largest sample-to-sample jump (not necessarily a fault)", xlabel="seconds", ylabel="full scale")
  axes[3].plot(records[1:, 0] / rate, np.diff(records[:, 2]) * 1000, label="callback interval")
  axes[3].plot(records[:, 0] / rate, records[:, 7] * 1000, label="callback work")
  axes[3].set(xlabel="seconds", ylabel="milliseconds")
  axes[3].legend()
  figure.savefig(prefix.with_suffix(".png"))
  plt.close(figure)
  print(json.dumps(report, indent=2))


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--host", help="SSH destination, e.g. comma@192.168.63.47")
  parser.add_argument("--output", type=Path, required=True, help="Local capture prefix, without extension")
  parser.add_argument("--analyze-only", action="store_true")
  parser.add_argument("--remote-root", default="/data/mic-lab/captures", help="Device scratch directory (avoid the small /tmp tmpfs)")
  parser.add_argument("--keep-remote", action="store_true")
  args, capture_args = parser.parse_known_args()
  if args.analyze_only:
    analyze(args.output)
    return
  if not args.host:
    parser.error("--host is required to capture")
  if args.output.with_suffix(".npz").exists():
    parser.error("output already exists; choose a new prefix to preserve the baseline")
  args.output.parent.mkdir(parents=True, exist_ok=True)
  remote_directory = args.remote_root.rstrip("/") + "/comma-mic-probe-" + uuid.uuid4().hex[:12]
  subprocess.run(["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=10", args.host,
                  "mkdir -p " + shlex.quote(args.remote_root) + " && mkdir -m 700 " + shlex.quote(remote_directory)], check=True)
  subprocess.run(["scp", "-q", str(Path(__file__).with_name("capture.py")), args.host + ":" + remote_directory + "/capture.py"], check=True)
  remote_prefix = remote_directory + "/capture"
  command = "source /etc/profile && cd /data/openpilot && " + shlex.join(
    ["python3", remote_directory + "/capture.py", remote_prefix, *capture_args])
  subprocess.run(["ssh", "-o", "BatchMode=yes", args.host, "bash -lc " + shlex.quote(command)], check=True)
  for extension in ("npz", "wav", "json"):
    subprocess.run(["scp", "-q", args.host + ":" + remote_prefix + "." + extension,
                    str(args.output.with_suffix("." + extension))], check=True)
  analyze(args.output)
  if not args.keep_remote:
    subprocess.run(["ssh", args.host, "rm -r -- " + shlex.quote(remote_directory)], check=True)


if __name__ == "__main__":
  main()
