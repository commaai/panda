import json
import wave
from pathlib import Path

import numpy as np

RESULTS = Path('/home/batman/tmp/c4-mic-results')
ROOT = RESULTS / 'reference-eq-01'
ROOT.mkdir(exist_ok=True)
RATE = 48000


def read(path):
  with wave.open(str(path)) as audio:
    return np.frombuffer(audio.readframes(audio.getnframes()), dtype='<i2').astype(float).reshape(-1, audio.getnchannels()) / 32768


def psd(samples):
  size = 16384
  window = np.hanning(size)
  spectra = []
  for channel in samples.T:
    frames = np.lib.stride_tricks.sliding_window_view(channel, size)[::8192]
    power = (abs(np.fft.rfft(frames * window, axis=1)) ** 2).mean(axis=0) / (RATE * np.sum(window ** 2))
    power[1:-1] *= 2
    spectra.append(power)
  return np.fft.rfftfreq(size, 1 / RATE), np.mean(spectra, axis=0)


source = read(RESULTS / 'phone-music-camera-01/phone-music.wav')
capture = read(RESULTS / 'phone-music-camera-01/captured.wav')
offset = 1.48
frequencies, source_power = psd(source[3*RATE:46*RATE])
_, capture_power = psd(capture[round((3+offset)*RATE):round((46+offset)*RATE)])
_, noise_power = psd(capture[round((49+offset)*RATE):])
anchor = (frequencies >= 500) & (frequencies < 2000)
level_offset = 10 * np.log10(source_power[anchor].sum() / capture_power[anchor].sum())
rows = []
for center in 1000 * 2 ** (np.arange(-15, 14) / 3):
  band = (frequencies >= center / 2**(1/6)) & (frequencies < center * 2**(1/6))
  reference = float(source_power[band].sum())
  recorded = float(capture_power[band].sum())
  quiet = float(noise_power[band].sum())
  rows.append({'center_hz': float(center), 'reference_minus_capture_db': float(10*np.log10(reference/recorded)-level_offset), 'capture_above_quiet_db': float(10*np.log10(recorded/quiet))})
np.savez(ROOT / 'spectra.npz', frequencies=frequencies, reference_power=source_power, capture_power=capture_power, noise_power=noise_power)
metrics = {'source': 'Actual file played in phone-music-camera-01; studio radio-edit reference', 'capture': 'phone-music-camera-01', 'aligned_source_seconds': [3,46], 'capture_offset_seconds': offset, 'reference_level_offset_db': float(level_offset), 'normalization_band_hz': [500,2000], 'bands': rows, 'limitation': 'Broadband spectral ratio includes phone, room, enclosure and mic. This is an example-specific tonal target, not an exact mic calibration.'}
(ROOT / 'measurement.json').write_text(json.dumps(metrics, indent=2)+'\n')
print(json.dumps(metrics, indent=2))
