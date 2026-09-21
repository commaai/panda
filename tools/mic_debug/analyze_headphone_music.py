import base64
import json
import wave
from pathlib import Path

import matplotlib
import numpy as np

matplotlib.use('Agg')
import matplotlib.pyplot as plt

RESULTS = Path('/home/batman/tmp/c4-mic-results')
ROOT = RESULTS / 'headphone-music-01'
BANDS = [(500, 2000), (4000, 8000), (8000, 12000), (12000, 14000), (14000, 16000), (16000, 18000), (18000, 20000), (20000, 22000)]


def read_wav(path):
  with wave.open(str(path)) as recording:
    assert recording.getsampwidth() == 2 and recording.getframerate() == 48000
    return np.frombuffer(recording.readframes(recording.getnframes()), dtype='<i2').reshape(-1, recording.getnchannels()) / 32768.


def spectrum(samples):
  size, hop, rate = 4096, 480, 48000
  window = np.hanning(size)
  spectra = []
  for channel in samples.T:
    blocks = np.lib.stride_tricks.sliding_window_view(channel, size)[::hop]
    power = abs(np.fft.rfft(blocks * window, axis=1)) ** 2 / (rate * np.sum(window ** 2))
    power[:, 1:-1] *= 2
    spectra.append(power)
  return (np.arange(len(power)) * hop + size / 2) / rate, np.fft.rfftfreq(size, 1 / rate), np.mean(spectra, axis=0)


def listening_file(samples, path):
  rms = np.sqrt(np.mean(samples ** 2))
  gain = min(10 ** (-24 / 20) / max(rms, 1e-20), .9 / max(abs(samples).max(), 1e-20))
  with wave.open(str(path), 'wb') as recording:
    recording.setparams((samples.shape[1], 2, 48000, 0, 'NONE', 'not compressed'))
    recording.writeframes(np.rint(samples * gain * 32767).astype('<i2').tobytes())
  return {'gain_db': float(20 * np.log10(gain)), 'result_rms_dbfs': float(20 * np.log10(rms * gain)), 'peak_dbfs': float(20 * np.log10(abs(samples).max() * gain))}


def main():
  source = read_wav(ROOT / 'headphone-music.wav')
  source_times, frequencies, source_power = spectrum(source)
  midband = (frequencies >= 500) & (frequencies < 2000)
  source_region = (source_times > 3) & (source_times < 46)
  reference = np.log(np.maximum(source_power[source_region][:, midband].sum(axis=1), 1e-20))
  reference -= reference.mean()
  signals = [('Source file (stereo power average)', source_times - 2, source_power, source_region)]
  metrics = {'dataset': 'headphone-music-01, compared with phone-music-camera-01', 'same_source_bytes': (ROOT / 'headphone-music.wav').read_bytes() == (RESULTS / 'phone-music-camera-01/phone-music.wav').read_bytes(), 'captures': {}, 'listening': {}}
  metrics['listening']['source'] = listening_file(source[2 * 48000:47 * 48000], ROOT / 'listen-source.wav')
  averaged = []
  for name, label in [('phone-music-camera-01', 'iPhone → comma four'), ('headphone-music-01', 'AirPods Max → comma four')]:
    capture = read_wav(RESULTS / name / 'captured.wav')
    times, _, power = spectrum(capture)
    envelope = np.log(np.maximum(power[:, midband].sum(axis=1), 1e-20))
    offsets = np.arange(0, 4, .01)
    correlations = []
    for offset in offsets:
      candidate = np.interp(source_times[source_region] + offset, times, envelope)
      candidate -= candidate.mean()
      correlations.append(float(np.dot(reference, candidate) / np.sqrt(np.dot(reference, reference) * np.dot(candidate, candidate))))
    offset = float(offsets[np.argmax(correlations)])
    assert max(correlations) > .4, 'Cannot confidently align music'
    region = (times > offset + 3) & (times < offset + 46)
    quiet = times > offset + 49
    assert quiet.sum() > 100
    bands = []
    for lower, upper in BANDS:
      band = (frequencies >= lower) & (frequencies < upper)
      active_db = float(10 * np.log10(power[region][:, band].mean(axis=0).sum() * (frequencies[1] - frequencies[0])))
      noise_db = float(10 * np.log10(power[quiet][:, band].mean(axis=0).sum() * (frequencies[1] - frequencies[0])))
      bands.append({'band_hz': [lower, upper], 'music_dbfs': active_db, 'quiet_dbfs': noise_db, 'rise_db': active_db-noise_db})
    raw = np.load(RESULTS / name / 'captured.npz')
    metrics['captures'][name] = {'source_offset_seconds': offset, 'envelope_correlation': max(correlations), 'bands': bands, 'input_overflows': int(raw['callbacks'][:, 5].sum()), 'input_underflows': int(raw['callbacks'][:, 6].sum()), 'rail_samples': int(np.count_nonzero(abs(raw['samples'].astype(np.int32)) >= 32767)), 'peak_dbfs': float(20*np.log10(abs(capture).max()))}
    signals.append((label, times - offset - 2, power, region))
    averaged.append((label, power[region].mean(axis=0), power[quiet].mean(axis=0)))
    clip = capture[round((offset + 2) * 48000):round((offset + 47) * 48000)]
    metrics['listening'][name] = listening_file(clip, ROOT / ('listen-' + name + '.wav'))
  figure, axes = plt.subplots(3, 1, figsize=(12, 11), constrained_layout=True)
  for axis, (label, times, power, region) in zip(axes, signals, strict=True):
    normalization = 10 * np.log10(power[region][:, midband].mean())
    heatmap = axis.pcolormesh(times, frequencies / 1000, 10 * np.log10(np.maximum(power.T, 1e-20)) - normalization, shading='auto', vmin=-85, vmax=15)
    axis.set(title=label, xlabel='Aligned music time (s)', ylabel='Frequency (kHz)', xlim=(0, 45), ylim=(0, 24))
  figure.colorbar(heatmap, ax=axes, label='PSD relative to each signal’s mean 500–2000 Hz PSD (dB)')
  figure.savefig(ROOT / 'music-comparison.png', dpi=150)
  plt.close(figure)
  figure, axis = plt.subplots(figsize=(12, 5), constrained_layout=True)
  for label, active, quiet in averaged:
    axis.plot(frequencies / 1000, 10 * np.log10(np.maximum(active, 1e-20)), label=label + ': music')
    axis.plot(frequencies / 1000, 10 * np.log10(np.maximum(quiet, 1e-20)), label=label + ': post-music quiet')
  axis.set(xlabel='Frequency (kHz)', ylabel='PSD (dBFS/Hz)', xlim=(0, 24), title='Absolute recorded spectra: music and quiet, without level normalization')
  axis.legend()
  figure.savefig(ROOT / 'music-noise.png', dpi=150)
  metrics['limitations'] = 'Uncalibrated speakers and different positions/volumes. Stereo playback may couple differently to the mono mic. This does not isolate microphone response. Heatmaps match 500–2000 Hz mean power; listening copies use only a constant gain to target -24 dBFS RMS, subject to peak headroom. No EQ or denoising. Raw captures retained.'
  metrics['placement_caveat'] = 'The user reported moving the AirPods; whether this happened between tests, during music, or both is not yet established. Placement was not controlled, so the weaker music result cannot be compared with the tone test as a fixed-position measurement.'
  (ROOT / 'metrics.json').write_text(json.dumps(metrics, indent=2) + '\n')
  page = '<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>AirPods versus iPhone music capture</title><style>body{font:17px/1.5 system-ui;max-width:1200px;margin:25px auto;padding:0 20px}img,audio{width:100%}td,th{padding:6px;text-align:right}</style><h1>Same music: AirPods versus iPhone</h1><p>Actual microphone recordings, same music file and pop-fix firmware. The AirPods recording followed an on-device spoken countdown, with an open earcup near the USB corner of the flat comma four. No correction processing changed.</p><p>' + metrics['limitations'] + '</p>'
  for title, filename in [('Source file', 'listen-source.wav'), ('Earlier iPhone → microphone', 'listen-phone-music-camera-01.wav'), ('New AirPods → microphone', 'listen-headphone-music-01.wav')]:
    page += '<h2>' + title + '</h2><audio controls preload="none" src="data:audio/wav;base64,' + base64.b64encode((ROOT / filename).read_bytes()).decode() + '"></audio>'
  for filename in ('music-comparison.png', 'music-noise.png'):
    page += '<img alt="' + filename + '" src="data:image/png;base64,' + base64.b64encode((ROOT / filename).read_bytes()).decode() + '">'
  page = page.replace('<h1>Same music: AirPods versus iPhone</h1>', '<h1>Same music: AirPods versus iPhone</h1><p><strong>Movement reported:</strong> ' + metrics['placement_caveat'] + '</p>')
  page += '<h2>Music band power above post-music quiet</h2><table><tr><th>Band (kHz)</th><th>iPhone (dB)</th><th>AirPods (dB)</th></tr>'
  phone = metrics['captures']['phone-music-camera-01']['bands']
  headphone = metrics['captures']['headphone-music-01']['bands']
  for first, second in zip(phone, headphone, strict=True):
    lower, upper = first['band_hz']
    page += f'<tr><td>{lower/1000:g}–{upper/1000:g}</td><td>{first["rise_db"]:.1f}</td><td>{second["rise_db"]:.1f}</td></tr>'
  page += '</table><p>Band increases alone do not prove recovered musical detail; narrow interferers and changed room noise can contribute. All audio and figures are embedded, with no external assets.</p>'
  (ROOT / 'index.html').write_text(page)
  print(json.dumps(metrics, indent=2))


if __name__ == '__main__':
  main()
