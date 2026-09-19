import json
import wave
from pathlib import Path

import matplotlib
import numpy as np

matplotlib.use('Agg')
import matplotlib.pyplot as plt

ROOT = Path('/home/batman/tmp/c4-mic-results')
OUTPUT = ROOT / 'tone-music-narrowband-01'
OUTPUT.mkdir(exist_ok=True)


def read_wav(path):
  with wave.open(str(path)) as recording:
    rate = recording.getframerate()
    samples = np.frombuffer(recording.readframes(recording.getnframes()), dtype='<i2').reshape(-1, recording.getnchannels()) / 32768.
  return rate, samples


def narrow_power(samples, rate, centers):
  size = 48000
  window = np.hanning(size)
  frequencies = np.fft.rfftfreq(size, 1 / rate)
  blocks = np.stack([samples[start:start + size] for start in range(0, len(samples) - size + 1, size // 2)])
  power = abs(np.fft.rfft(blocks * window[None, :, None], axis=1)) ** 2 / (rate * np.sum(window ** 2))
  power[:, 1:-1] *= 2
  return np.array([power[:, abs(frequencies - center) < 25].sum(axis=1).mean() * rate / size for center in centers])


rate, music_source = read_wav(ROOT / 'phone-music-camera-01/phone-music.wav')
_, music_capture = read_wav(ROOT / 'phone-music-camera-01/captured.wav')
_, tone_source = read_wav(ROOT / 'phone-levels-camera-01/phone-levels.wav')
_, tone_capture = read_wav(ROOT / 'phone-levels-camera-01/captured.wav')
prior = json.loads((ROOT / 'phone-camera-report/metrics.json').read_text())
centers = np.array([14000, 16000, 17000, 18000, 19000, 20000])
offset = prior['music_source_offset_seconds']
music_start, music_end = 3, 46
music_source_power = narrow_power(music_source[music_start * rate:music_end * rate], rate, centers)
music_capture_power = narrow_power(music_capture[round((music_start + offset) * rate):round((music_end + offset) * rate)], rate, centers)
noise_power = narrow_power(music_capture[round((49 + offset) * rate):], rate, centers)
rows = []
for index, frequency in enumerate(centers):
  measured = next(item for item in prior['levels'] if item['peak'] == .2 and item['frequency_hz'] == frequency)
  # The central 0.6 s contains no fade. Exact sine amplitude determines source band power.
  source_tone_db = float(10 * np.log10(.2 ** 2 / 2))
  row = {'frequency_hz': int(frequency), 'source_tone_dbfs': source_tone_db,
         'source_music_band_dbfs': float(10 * np.log10(music_source_power[index])),
         'capture_tone_band_dbfs': measured['tone_band_dbfs'],
         'capture_music_band_dbfs': float(10 * np.log10(music_capture_power[index])),
         'capture_quiet_band_dbfs': float(10 * np.log10(noise_power[index]))}
  row['source_tone_minus_music_db'] = source_tone_db - row['source_music_band_dbfs']
  row['capture_tone_minus_music_db'] = row['capture_tone_band_dbfs'] - row['capture_music_band_dbfs']
  row['capture_music_above_noise_db'] = row['capture_music_band_dbfs'] - row['capture_quiet_band_dbfs']
  rows.append(row)
metadata = {'dataset': 'phone-music-camera-01 and phone-levels-camera-01, identical firmware',
            'band': 'strictly within ±25 Hz of each listed frequency; same bandwidth, absolute full-scale convention',
            'source_music_convention': 'mean channel power of the stereo file, not an acoustic measurement',
            'source_tone_convention': 'calculated RMS for 0.2-peak sine; file rounding at 16 bits is negligible here',
            'music_windows': {'source_seconds': [music_start, music_end], 'capture_offset_seconds': offset},
            'limitations': 'No gain correction for the phone speaker, room or mic. Narrow-band tone levels use previous central 0.6 s Hann measurements; music/noise use averaged 1 s Hann windows. Source and capture spectral differences are not a calibrated transfer function.',
            'rows': rows}
(OUTPUT / 'metrics.json').write_text(json.dumps(metadata, indent=2) + '\n')
figure, axes = plt.subplots(2, 1, figsize=(11, 8), constrained_layout=True)
axes[0].plot(centers / 1000, [item['source_tone_dbfs'] for item in rows], marker='o', label='Played tone (0.2 peak)')
axes[0].plot(centers / 1000, [item['source_music_band_dbfs'] for item in rows], marker='o', label='Played music, average in same band')
axes[0].set(title='Input files: isolated tones carry much more energy in each narrow band', ylabel='Band RMS (dBFS)', xlabel='Frequency (kHz)')
axes[0].legend()
for key, label in [('capture_tone_band_dbfs', 'Recorded tone'), ('capture_music_band_dbfs', 'Recorded music'), ('capture_quiet_band_dbfs', 'Recorded quiet interval')]:
  axes[1].plot(centers / 1000, [item[key] for item in rows], marker='o', label=label)
axes[1].set(title='Actual microphone: music versus tone versus noise, all in ±25 Hz bands', ylabel='Band RMS (dBFS)', xlabel='Frequency (kHz)')
axes[1].legend()
figure.savefig(OUTPUT / 'narrowband-comparison.png', dpi=150)
print(json.dumps(metadata))
