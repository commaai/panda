"""Broad, reference-derived tonal matching of one real recording; not mic calibration."""
import base64
import hashlib
import io
import json
import subprocess
import wave
from pathlib import Path

import matplotlib
import numpy as np

from make_clarity_comparison import process

matplotlib.use('Agg')
import matplotlib.pyplot as plt

RESULTS = Path('/home/batman/tmp/c4-mic-results')
ROOT = RESULTS / 'reference-eq-01'
RATE = 48000
FFMPEG = '/usr/bin/ffmpeg'


def read(path):
  with wave.open(str(path)) as audio:
    return np.frombuffer(audio.readframes(audio.getnframes()), dtype='<i2').astype(float).reshape(-1, audio.getnchannels()) / 32768


def loudness(samples):
  channels = samples.shape[1] if samples.ndim == 2 else 1
  result = subprocess.run([FFMPEG, '-hide_banner', '-f', 'f64le', '-ar', str(RATE), '-ac', str(channels), '-i', 'pipe:0', '-af', 'loudnorm=I=-24:TP=-3:print_format=json', '-f', 'null', '-'], input=samples.astype('<f8').tobytes(), capture_output=True, check=True)
  output = result.stderr.decode()
  value = json.loads(output[output.rfind('{'):output.rfind('}')+1])
  return {key: float(item) for key, item in value.items() if key.startswith('input_')}


def wav_bytes(samples):
  channels = samples.shape[1] if samples.ndim == 2 else 1
  buffer = io.BytesIO()
  with wave.open(buffer, 'wb') as audio:
    audio.setparams((channels, 2, RATE, 0, 'NONE', 'not compressed'))
    audio.writeframes(np.rint(samples * 32767).astype('<i2').tobytes())
  return buffer.getvalue()


def main():
  measurement = json.loads((ROOT / 'measurement.json').read_text())
  centers = np.array([row['center_hz'] for row in measurement['bands']])
  differences = np.array([row['reference_minus_capture_db'] for row in measurement['bands']])
  confidence = np.array([row['capture_above_quiet_db'] for row in measurement['bands']])
  # Smooth log-spaced measurements, avoiding narrow room-response inversions.
  smoothed = np.convolve(np.pad(differences, 1, mode='edge'), [.25, .5, .25], mode='valid')
  reliability = np.clip((confidence - 3) / 7, 0, 1)
  edge = np.interp(centers, [0, 90, 150, 10000, 14000, 24000], [0, 0, 1, 1, 0, 0])
  desired = np.clip(smoothed, -6, 24) * reliability * edge
  size, taps = 32768, 2049
  frequencies = np.fft.rfftfreq(size, 1 / RATE)
  target_db = np.interp(np.log2(np.maximum(frequencies, 1)), np.log2(centers), desired, left=0, right=0)
  source = read(RESULTS / 'phone-music-camera-01/captured.wav')[:, 0]
  source = source[round(3.48 * RATE):round(48.48 * RATE)]
  work_gain = .5 / abs(source).max()
  original = source * work_gain
  reference = read(RESULTS / 'phone-music-camera-01/phone-music.wav')[2 * RATE:47 * RATE]
  gentle_body = process(original, 'equalizer=f=3200:t=q:w=0.75:g=3,equalizer=f=7500:t=q:w=0.8:g=3')
  variants = [('original', 'Original mic recording', original), ('gentle_body', 'Gentle EQ — bass retained', gentle_body)]
  curves = []
  for strength, key, title in [(.5, 'reference_half', 'Reference fit — halfway'), (1., 'reference_full', 'Reference fit — closer match')]:
    impulse = np.fft.fftshift(np.fft.irfft(10 ** (target_db * strength / 20), n=size))
    half = taps // 2
    kernel = impulse[size // 2-half:size // 2+half+1] * np.hamming(taps)
    filtered = np.convolve(original, kernel, mode='same')
    assert len(filtered) == len(original) and np.all(np.isfinite(filtered))
    np.save(ROOT / (key + '-taps.npy'), kernel)
    actual = 20 * np.log10(np.maximum(abs(np.fft.rfft(kernel, n=size)), 1e-20))
    curves.append((title, actual))
    variants.append((key, title, filtered))
  variants.append(('reference', 'Reference source — stereo', reference))
  measurements = {key: loudness(samples) for key, _, samples in variants}
  target_lufs = min(-24., min(row['input_i'] - row['input_tp'] - 3 for row in measurements.values()))
  metadata = {'measurement': measurement, 'source_sha256': hashlib.sha256((RESULTS / 'phone-music-camera-01/captured.wav').read_bytes()).hexdigest(), 'working_gain': work_gain, 'loudness_target_lufs': target_lufs, 'filter_taps': taps, 'filter_alignment': 'Centered offline convolution; real-time implementation/latency unvalidated', 'design': 'Log-spaced one-third-octave power-ratio measurements, three-band smoothing, gain limited to -6/+24 dB, tapered out below 90–150 Hz and above 10–14 kHz, attenuated where capture is within 3–10 dB of quiet. Level anchor 500–2000 Hz.', 'band_targets': [{'center_hz': float(center), 'raw_difference_db': float(raw), 'full_target_db': float(target)} for center, raw, target in zip(centers, differences, desired, strict=True)], 'variants': [], 'limitations': 'This matches the tonal balance of this one phone+room+enclosure+mic capture toward the source. It is not an exact microphone calibration or waveform reconstruction. Reference is stereo, capture mono. Noise remains and is amplified by positive EQ. No source samples are mixed into processed microphone clips. No synthesis or denoising in these versions.'}
  clips = {}
  for key, title, samples in variants:
    gain = 10 ** ((target_lufs - measurements[key]['input_i']) / 20)
    signal = samples * gain
    assert abs(signal).max() < .9
    encoded = wav_bytes(signal)
    (ROOT / (key + '.wav')).write_bytes(encoded)
    channels = samples.shape[1] if samples.ndim == 2 else 1
    decoded = np.frombuffer(encoded[44:], dtype='<i2').astype(float).reshape(-1, channels) / 32768
    verified = loudness(decoded)
    assert abs(verified['input_i'] - target_lufs) < .15 and verified['input_tp'] <= -2.8
    metadata['variants'].append({'name': key, 'title': title, 'channels': channels, 'integrated_lufs': verified['input_i'], 'true_peak_dbfs': verified['input_tp'], 'playback_gain_db': float(20*np.log10(gain))})
    clips[key] = {'title': title, 'uri': 'data:audio/wav;base64,' + base64.b64encode(encoded).decode()}
  figure, axes = plt.subplots(2, 1, figsize=(12, 8), constrained_layout=True)
  axes[0].semilogx(centers, differences, marker='o', label='Measured reference/capture tonal difference')
  for title, actual in curves:
    axes[0].semilogx(frequencies[1:], actual[1:], label=title + ': actual applied filter')
  axes[0].set(xlabel='Frequency (Hz)', ylabel='Relative gain (dB)', xlim=(30, 22000), title='Reference-derived correction, before playback loudness matching')
  axes[0].legend()
  axes[1].semilogx(centers, confidence, marker='o', label='Recorded music band above post-music quiet')
  axes[1].semilogx(centers, desired, marker='o', label='Full target after smoothing, limits and taper')
  axes[1].set(xlabel='Frequency (Hz)', ylabel='dB', xlim=(30, 22000), title='Avoid trying to invert missing sub-bass and the noise-dominated upper end')
  axes[1].legend()
  figure.savefig(ROOT / 'reference-fit.png', dpi=150)
  plt.close(figure)
  (ROOT / 'metrics.json').write_text(json.dumps(metadata, indent=2) + '\n')
  page = '''<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Reference-derived mic EQ</title><style>body{font:17px/1.5 system-ui;max-width:1150px;margin:25px auto;padding:0 20px}button{font:inherit;padding:12px;margin:5px;border:1px solid #888;border-radius:7px;cursor:pointer}button[aria-pressed=true]{background:#184b77;color:white}audio,img{width:100%}.note{background:#eee;padding:14px}</style><h1>Reference-derived EQ: same iPhone recording</h1><p>Click to play or switch at the same time. These are new offline versions of the actual iPhone-to-comma-four recording; no new capture or firmware change.</p><div id="buttons"></div><h2 id="playing">Original mic recording</h2><audio id="player" controls preload="none"></audio><p id="error" role="status"></p><button id="restart">Restart this version</button><div class="note"><strong>Gentle EQ — bass retained:</strong> keeps the earlier broad treble EQ, removes its low-bass cut.<br><strong>Reference fit — halfway:</strong> half of a smoothed, measured tonal correction.<br><strong>Reference fit — closer match:</strong> stronger reference-derived correction; can reveal more hiss and room noise.<br><strong>Reference source:</strong> the stereo file played on the iPhone, included only as a listening target. It is not mixed into any microphone version.</div><p>The reference/capture difference includes the phone speaker, room, case opening, microphone and processing. It cannot uniquely calibrate the mic or restore the original waveform. The reference has relatively more bass and upper treble than this capture after matching 500–2000 Hz power. This measured shape differs from a generic bass cut plus treble boost.</p>'''
  page += '<p>' + metadata['design'] + '</p>'
  page += f'<p>Every clip is matched to {target_lufs:.2f} LUFS with constant gain only and approximately 3 dB or more true-peak headroom. No compression, limiting, denoising or synthetic harmonics. No boost can improve the signal-to-noise ratio already recorded.</p>'
  page += '<img alt="Measured reference difference and applied EQ" src="data:image/png;base64,' + base64.b64encode((ROOT / 'reference-fit.png').read_bytes()).decode() + '">'
  page += '<p>All audio and figures are embedded. Filters were fitted and auditioned on this same music example; generalization to other recordings is not established.</p>'
  ui = (RESULTS / 'clarity-comparison-01/index.html').read_text().split(';const player=document.getElementById', 1)[1]
  page += '<script>const clips=' + json.dumps(clips) + ';const player=document.getElementById' + ui
  (ROOT / 'index.html').write_text(page)
  print(json.dumps({'target_lufs': target_lufs, 'variants': metadata['variants'], 'max_full_target_db': float(desired.max())}, indent=2))


if __name__ == '__main__':
  main()
