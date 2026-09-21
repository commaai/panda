#!/usr/bin/env python3
"""Measure the actual C microphone resampler with synthetic noiseless tones."""

import argparse
import ctypes
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

import numpy as np

from check_resampler import HARDWARE_STUB, TEST_DRIVER


def main():
  parser = argparse.ArgumentParser(description="Measure the actual C mic resampler with synthetic noiseless tones.")
  parser.add_argument('sound_header', type=Path)
  parser.add_argument('--output', type=Path, required=True)
  parser.add_argument('--frequencies', type=float, nargs='+', default=[1000, 4000, 7500])
  args = parser.parse_args()
  if not all(0 < frequency < 23000 for frequency in args.frequencies):
    parser.error('frequencies must be between 0 and 23000 Hz')
  source = args.sound_header.read_text()
  preamble = source[source.index('#define SOUND_RX_BUF_SIZE') : source.index('void sound_tick')]
  handlers = source[source.index('// Count complete input') : source.index('// Playback processing')]
  driver = TEST_DRIVER.replace('double ratio, int16_t *out', 'double ratio, double frequency, int16_t *out')
  driver = driver.replace('1000.0 /', 'frequency /')
  results = []
  with tempfile.TemporaryDirectory(prefix='mic-quality-') as directory:
    code = Path(directory) / 'quality.c'
    library_path = Path(directory) / 'quality.so'
    code.write_text(HARDWARE_STUB + preamble + handlers + driver)
    subprocess.run(['cc', '-shared', '-fPIC', '-O2', str(code), '-lm', '-o', str(library_path)], check=True)
    library = ctypes.CDLL(str(library_path))
    library.render_test.argtypes = [ctypes.c_uint32, ctypes.c_double, ctypes.c_double, ctypes.POINTER(ctypes.c_int16)]
    library.render_test.restype = None
    for ratio in [1.0, 1000 / 1001]:
      for frequency in args.frequencies:
        library.reset_test()
        output = np.empty((4000, 512), dtype=np.int16)
        for block in range(len(output)):
          produced = 4096 + round(block * 512 * ratio)
          library.render_test(produced, ratio, frequency, output[block].ctypes.data_as(ctypes.POINTER(ctypes.c_int16)))
        samples = output.ravel()[-96000:].astype(float)
        phase = 2 * np.pi * frequency * np.arange(len(samples)) / 48000
        basis = np.column_stack((np.sin(phase), np.cos(phase), np.ones(len(samples))))
        coefficients = np.linalg.lstsq(basis, samples, rcond=None)[0]
        amplitude = np.hypot(*coefficients[:2])
        residual = samples - basis @ coefficients
        results.append(
          {
            'input_ratio': ratio,
            'tone_hz': frequency,
            'gain_db': float(20 * np.log10(amplitude / 30000)),
            'tone_to_residual_db': float(20 * np.log10(amplitude / np.sqrt(2) / np.std(residual))),
            'scope': 'Actual C interpolator; noiseless input; last two seconds after 40 s settling; quantization and rate-control error included.',
          }
        )
  report = {'source_sha256': hashlib.sha256(source.encode()).hexdigest(), 'results': results}
  args.output.write_text(json.dumps(report, indent=2) + '\n')
  print(json.dumps(results, indent=2))


if __name__ == "__main__":
  main()
