#!/usr/bin/env python3
"""Compile the actual Panda mic handlers against simulated DMA clocks and samples."""
import argparse
import ctypes
import json
import subprocess
import tempfile
from pathlib import Path

import numpy as np


HARDWARE_STUB = '''
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#define DMA_SxCR_CT 1U
#define DMA_SxCR_CT_Pos 0U
#define BDMA_CCR_CT 1U
#define BDMA_CCR_CT_Pos 0U
#define BDMA_IFCR_CGIF1 1U
struct dma_stream { uint32_t CR; uint32_t NDTR; } stream;
struct dma_flags { uint32_t LIFCR; } dma;
struct bdma_channel { uint32_t CCR; } channel;
struct bdma_flags { uint32_t IFCR; } bdma;
#define DMA1_Stream0 (&stream)
#define DMA1 (&dma)
#define BDMA_Channel1 (&channel)
#define BDMA (&bdma)
'''

TEST_DRIVER = '''
void reset_test(void) {
  mic_resampler_ready = false;
  mic_buffer_count = MIC_SKIP_BUFFERS + 2U;
  mic_idle_count = 4U;
}
void render_test(uint32_t produced, double ratio, int16_t *out) {
  for (uint32_t sample = produced - MIC_RING_SAMPLES; sample < produced; sample++) {
    uint32_t index = sample % MIC_RING_SAMPLES;
    int16_t value = (int16_t)(30000.0 * sin(sample * 6.283185307179586 * 1000.0 / (48000.0 * ratio)));
    mic_rx_buf[index / MIC_RX_BUF_SIZE][index % MIC_RX_BUF_SIZE] = ((uint32_t)(uint16_t)value) << 16U;
  }
  stream.CR = (produced % MIC_RING_SAMPLES) / MIC_RX_BUF_SIZE;
  stream.NDTR = MIC_RX_BUF_SIZE - (produced % MIC_RX_BUF_SIZE);
  BDMA_Channel1_IRQ_Handler();
  for (uint32_t sample = 0U; sample < MIC_RX_BUF_SIZE; sample++) {
    out[sample] = (int16_t)mic_tx_buf[1][sample * 2U];
  }
}
'''


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('sound_header', type=Path)
  parser.add_argument('--output', type=Path)
  args = parser.parse_args()
  source = args.sound_header.read_text()
  preamble = source[source.index('#define SOUND_RX_BUF_SIZE'):source.index('void sound_tick')]
  handlers = source[source.index('// Count complete input'):source.index('// Playback processing')]
  results = []
  with tempfile.TemporaryDirectory(prefix='mic-resampler-') as temporary:
    directory = Path(temporary)
    code = directory / 'resampler.c'
    shared_library = directory / 'resampler.so'
    code.write_text(HARDWARE_STUB + preamble + handlers + TEST_DRIVER)
    subprocess.run(['cc', '-shared', '-fPIC', '-O2', '-fsanitize=undefined', '-fno-sanitize-recover=undefined',
                    str(code), '-lm', '-o', str(shared_library)], check=True)
    library = ctypes.CDLL(str(shared_library))
    library.render_test.argtypes = [ctypes.c_uint32, ctypes.c_double, ctypes.POINTER(ctypes.c_int16)]
    library.render_test.restype = None
    library.reset_test.restype = None
    for ratio in (0.998, 0.999, 1.0, 1.00002, 1.002):
      library.reset_test()
      random = np.random.default_rng(1)
      output = np.empty((30000, 512), dtype=np.int16)
      for block in range(len(output)):
        produced = 4096 + round((block * 512 + int(random.integers(-8, 9))) * ratio)
        library.render_test(produced, ratio, output[block].ctypes.data_as(ctypes.POINTER(ctypes.c_int16)))
      samples = output.ravel().astype(np.float64)
      maximum_jump = float(np.max(np.abs(np.diff(samples))))
      # This 30k-amplitude 1 kHz sine changes by at most ~3925 counts/sample.
      # A repeated block or substantial read-position reset produces a larger jump.
      assert maximum_jump < 4300, (ratio, maximum_jump)
      tail = samples[-480000:]
      frequencies = np.fft.rfftfreq(len(tail), 1 / 48000)
      measured = float(frequencies[np.argmax(np.abs(np.fft.rfft(tail)))])
      assert abs(measured - 1000) < 0.11, (ratio, measured)
      results.append({'input_ratio': ratio, 'seconds': len(samples) / 48000,
                      'maximum_sample_jump': maximum_jump, 'tone_hz': measured})
  serialized = json.dumps(results, indent=2) + '\n'
  print(serialized)
  if args.output:
    args.output.write_text(serialized)


if __name__ == '__main__':
  main()
