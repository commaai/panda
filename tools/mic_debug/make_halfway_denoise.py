"""Noise-profile audition with the existing halfway EQ; no synthetic harmonics."""
import base64
import hashlib
import io
import json
import wave
from pathlib import Path

import matplotlib
import numpy as np

from make_clarity_comparison import loudness, spectrum, wav_bytes

matplotlib.use('Agg')
import matplotlib.pyplot as plt

RESULTS = Path('/home/batman/tmp/c4-mic-results')
ROOT = RESULTS / 'clarity-comparison-01'
REFERENCE = RESULTS / 'reference-eq-01'
RATE = 48000
SIZE = 2048
HOP = 512


def smooth(values, length, axis):
  padding = [(0, 0)] * values.ndim
  padding[axis] = (length // 2, length // 2)
  padded = np.pad(values, padding, mode='edge')
  return np.lib.stride_tricks.sliding_window_view(padded, length, axis=axis).mean(axis=-1)


def denoise(samples, profile):
  window = np.hanning(SIZE)
  padded = np.pad(samples, (SIZE, SIZE))
  frames = np.lib.stride_tricks.sliding_window_view(padded, SIZE)[::HOP]
  transformed = np.fft.rfft(frames * window, axis=1)
  profile_frames = np.lib.stride_tricks.sliding_window_view(profile, SIZE)[::HOP]
  noise = (abs(np.fft.rfft(profile_frames * window, axis=1)) ** 2).mean(axis=0)
  power = smooth(smooth(abs(transformed) ** 2, 9, 1), 5, 0)
  noise = smooth(noise, 9, 0)
  estimated_gain = np.sqrt(np.maximum(1 - noise / np.maximum(power, 1e-24), 0))
  floor = 10 ** (-6 / 20)
  gain = smooth(smooth(np.maximum(estimated_gain, floor), 9, 1), 5, 0)
  frequency = np.fft.rfftfreq(SIZE, 1 / RATE)
  weight = np.interp(frequency, [0, 2000, 3500, RATE/2], [0, 0, 1, 1])
  gain = 1 + (gain - 1) * weight
  reconstructed = np.fft.irfft(transformed * gain, n=SIZE, axis=1) * window
  output = np.zeros(len(padded))
  denominator = np.zeros(len(padded))
  for index, frame in enumerate(reconstructed):
    offset = index * HOP
    output[offset:offset+SIZE] += frame
    denominator[offset:offset+SIZE] += window ** 2
  output /= np.maximum(denominator, 1e-15)
  return output[SIZE:SIZE+len(samples)]


with wave.open(str(RESULTS / 'phone-music-camera-01/captured.wav')) as audio:
  assert audio.getframerate() == RATE and audio.getnchannels() == 1
  raw = np.frombuffer(audio.readframes(audio.getnframes()), dtype='<i2').astype(float) / 32768
assert len(raw) > 57 * RATE
profile = raw[round(50.48*RATE):54*RATE]
cleaned = denoise(raw, profile)
# A unity/no-noise input must reconstruct exactly; this also checks alignment and boundaries.
identity = denoise(raw[:RATE], np.zeros(RATE))
assert np.max(abs(identity - raw[:RATE])) < 1e-12
kernel = np.load(REFERENCE / 'reference_half-taps.npy')
metadata = json.loads((ROOT / 'metrics.json').read_text())
reference_metadata = json.loads((REFERENCE / 'metrics.json').read_text())
interval = slice(round(3.48*RATE), round(48.48*RATE))
processed = np.convolve(cleaned[interval] * reference_metadata['working_gain'], kernel, mode='same')
measured = loudness(processed)
gain = 10 ** ((metadata['loudness_target_lufs'] - measured['input_i']) / 20)
processed *= gain
assert abs(processed).max() < .9
payload = wav_bytes(processed)
with wave.open(io.BytesIO(payload)) as audio:
  decoded = np.frombuffer(audio.readframes(audio.getnframes()), dtype='<i2').astype(float) / 32768
verified = loudness(decoded)
assert abs(verified['input_i'] - metadata['loudness_target_lufs']) < .1
assert verified['input_tp'] < -2.9
key = 'reference_half_denoise'
title = 'Reference fit — halfway + light denoise'
(ROOT / (key + '.wav')).write_bytes(payload)

spectra = np.load(REFERENCE / 'spectra.npz')
frequency = spectra['frequencies']
response = abs(np.fft.rfft(kernel, n=(len(frequency)-1)*2)) ** 2
rows = []
quiet_before = np.convolve(raw[54*RATE:57*RATE], kernel, mode='same')
quiet_after = np.convolve(cleaned[54*RATE:57*RATE], kernel, mode='same')
quiet_frequency, before_power = spectrum(quiet_before)
_, after_power = spectrum(quiet_after)
for lower, upper in [(1000,3000),(3000,6000),(6000,10000),(10000,14000),(14000,20000)]:
  band = (frequency >= lower) & (frequency < upper)
  quiet_band = (quiet_frequency >= lower) & (quiet_frequency < upper)
  noise = spectra['noise_power'][band]
  rows.append({'band_hz':[lower,upper], 'eq_noise_boost_db':float(10*np.log10(np.sum(noise*response[band])/noise.sum())), 'music_above_quiet_db':float(10*np.log10(spectra['capture_power'][band].sum()/noise.sum())), 'held_out_quiet_reduction_db':float(10*np.log10(before_power[quiet_band].sum()/after_power[quiet_band].sum()))})
record = {'title':title, 'name':key, 'integrated_lufs':verified['input_i'], 'true_peak_dbfs':verified['input_tp'], 'bands':rows, 'profile_capture_seconds':[50.48,54], 'held_out_quiet_seconds':[54,57], 'method':'2048-sample Hann STFT, 512 hop; noise-power subtraction gain, bounded to at most 6 dB attenuation; 9-bin / 5-frame smoothing; no attenuation below 2 kHz, full effect above 3.5 kHz. Existing halfway EQ follows. Constant playback gain.', 'limitations':'Post-music room noise is an estimate, not isolated microphone self-noise. Quiet reduction is measured on withheld quiet, not proof of equal reduction underneath music. Denoising can soften detail or cause watery artifacts. No new capture or synthesis.', 'preserved_halfway_sha256':hashlib.sha256((ROOT/'reference_half.wav').read_bytes()).hexdigest()}
(ROOT / 'halfway-denoise.json').write_text(json.dumps(record, indent=2)+'\n')
metadata['halfway_denoise'] = record
metadata['latest_listener_feedback'] += ' User strongly preferred Reference fit halfway but heard mid/high hiss; added a separate bounded noise-profile audition without changing that favorite.'
(ROOT/'metrics.json').write_text(json.dumps(metadata,indent=2)+'\n')
figure, axes = plt.subplots(2,1,figsize=(11,8),constrained_layout=True)
axes[0].plot(frequency/1000, 10*np.log10(np.maximum(spectra['noise_power'],1e-24)), label='Recorded post-music quiet')
axes[0].plot(frequency/1000, 10*np.log10(np.maximum(spectra['noise_power']*response,1e-24)), label='Same quiet through halfway EQ')
axes[0].set(xlim=(1,20),xlabel='Frequency (kHz)',ylabel='PSD (dBFS/Hz)',title='Where halfway EQ raises existing background noise; before playback gain')
axes[0].legend()
axes[1].plot(quiet_frequency/1000,10*np.log10(np.maximum(before_power,1e-24)),label='Halfway EQ: held-out quiet')
axes[1].plot(quiet_frequency/1000,10*np.log10(np.maximum(after_power,1e-24)),label='With light denoise: same held-out quiet')
axes[1].set(xlim=(1,20),xlabel='Frequency (kHz)',ylabel='PSD (dBFS/Hz)',title='Noise reduction on 54–57 seconds; profile learned at 50.48–54 seconds')
axes[1].legend()
figure.savefig(ROOT/'halfway-hiss.png',dpi=130)
plt.close(figure)
page=(ROOT/'index.html').read_text()
start=page.index('const clips=')+len('const clips=')
end=page.index(';const player=',start)
clips=json.loads(page[start:end])
merged={}
for name,clip in clips.items():
  if name != key:
    merged[name]=clip
  if name == 'reference_half':
    merged[key]={'title':title,'uri':'data:audio/wav;base64,'+base64.b64encode(payload).decode()}
page=page[:start]+json.dumps(merged)+page[end:]
if 'id="hiss-note"' not in page:
  note='<div class="note" id="hiss-note"><strong>Halfway + light denoise:</strong> keeps the measured halfway EQ and adds mild noise-profile suppression above 2–3.5 kHz, capped at 6 dB. The original halfway version is unchanged. Its EQ raises background noise about 11 dB at 6–10 kHz, versus 0.5 dB at 14–20 kHz. This points toward lower treble as the added hiss source, without proving exactly what the listener hears. Denoising may soften detail or add watery artifacts; preference is pending.</div>'
  page=page.replace('<div id="buttons"></div>','<div id="buttons"></div>'+note)
  page=page.replace('<script>const clips=','<h2>Hiss measurements</h2><img alt="Halfway EQ noise boost and held-out quiet denoising" src="data:image/png;base64,'+base64.b64encode((ROOT/'halfway-hiss.png').read_bytes()).decode()+'"><script>const clips=')
  page=page.replace('No limiting, compression or denoising.', 'No limiting or compression. Only the explicitly labeled light-denoise version uses noise suppression.')
(ROOT/'index.html').write_text(page)
print(json.dumps(record,indent=2))
