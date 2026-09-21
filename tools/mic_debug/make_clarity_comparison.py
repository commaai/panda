"""Offline, loudness-matched EQ and explicitly synthetic harmonic auditions."""
import base64
import hashlib
import io
import json
import subprocess
import wave
from pathlib import Path

import matplotlib
import numpy as np

matplotlib.use('Agg')
import matplotlib.pyplot as plt

ROOT = Path('/home/batman/tmp/c4-mic-results/clarity-comparison-01')
SOURCE = Path('/home/batman/tmp/c4-mic-results/phone-music-camera-01/captured.wav')
FFMPEG = '/usr/bin/ffmpeg'
RATE = 48000
MILD = 'bass=f=200:t=q:w=0.707:g=-2,equalizer=f=3200:t=q:w=0.75:g=3,equalizer=f=7500:t=q:w=0.8:g=3'
STRONG = 'bass=f=250:t=q:w=0.707:g=-3,equalizer=f=3200:t=q:w=0.7:g=5,equalizer=f=7500:t=q:w=0.8:g=6'
WET = 'highpass=f=2500:p=2,lowpass=f=6500:p=2,aresample=192000,aexciter=freq=3000:ceil=14000:drive=6:amount=1:listen=1,highpass=f=6500:p=2,lowpass=f=14000:p=2,aresample=48000'


def process(samples, filters):
  result = subprocess.run([FFMPEG, '-v', 'error', '-f', 'f64le', '-ar', str(RATE), '-ac', '1', '-i', 'pipe:0', '-af', filters, '-f', 'f64le', '-ar', str(RATE), 'pipe:1'], input=samples.astype('<f8').tobytes(), capture_output=True, check=True)
  output = np.frombuffer(result.stdout, dtype='<f8').copy()
  assert len(output) == len(samples) and np.all(np.isfinite(output))
  return output


def harmonic_input_band(samples):
  size, taps = 16384, 513
  frequencies = np.fft.rfftfreq(size, 1 / RATE)
  target = np.interp(frequencies, [0, 1800, 2500, 4500, 5500, 24000], [0, 0, 1, 1, 0, 0])
  impulse = np.fft.fftshift(np.fft.irfft(target, n=size))
  half = taps // 2
  kernel = impulse[size // 2-half:size // 2+half+1] * np.hamming(taps)
  return np.convolve(samples, kernel, mode='same')


def shape_harmonic_band(samples):
  size, taps = 16384, 513
  frequencies = np.fft.rfftfreq(size, 1 / RATE)
  target = np.interp(frequencies, [0, 5500, 7000, 12000, 14500, 24000], [0, 0, 1, 1, 0, 0])
  impulse = np.fft.fftshift(np.fft.irfft(target, n=size))
  half = taps // 2
  kernel = impulse[size // 2-half:size // 2+half+1] * np.hamming(taps)
  return np.convolve(samples, kernel, mode='same')


def loudness(samples):
  result = subprocess.run([FFMPEG, '-hide_banner', '-f', 'f64le', '-ar', str(RATE), '-ac', '1', '-i', 'pipe:0', '-af', 'loudnorm=I=-24:TP=-3:print_format=json', '-f', 'null', '-'], input=samples.astype('<f8').tobytes(), capture_output=True, check=True)
  output = result.stderr.decode()
  values = json.loads(output[output.rfind('{'):output.rfind('}')+1])
  return {key: float(value) for key, value in values.items() if key.startswith('input_')}


def wav_bytes(samples):
  buffer = io.BytesIO()
  with wave.open(buffer, 'wb') as recording:
    recording.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
    recording.writeframes(np.rint(samples * 32767).astype('<i2').tobytes())
  return buffer.getvalue()


def spectrum(samples):
  size = 8192
  window = np.hanning(size)
  frames = np.lib.stride_tricks.sliding_window_view(samples, size)[::4096]
  power = (abs(np.fft.rfft(frames * window, axis=1)) ** 2).mean(axis=0) / (RATE * np.sum(window ** 2))
  power[1:-1] *= 2
  return np.fft.rfftfreq(size, 1 / RATE), power


def main():
  ROOT.mkdir(exist_ok=True)
  with wave.open(str(SOURCE)) as recording:
    assert recording.getframerate() == RATE and recording.getnchannels() == 1
    source = np.frombuffer(recording.readframes(recording.getnframes()), dtype='<i2').astype(float) / 32768
  offset = 1.48
  source = source[round((offset + 2) * RATE):round((offset + 47) * RATE)]
  work_gain = .5 / abs(source).max()
  original = source * work_gain
  mild = process(original, MILD)
  strong = process(original, STRONG)
  wet = shape_harmonic_band(process(mild, WET))
  wet_gain = np.sqrt(np.mean(mild ** 2) / np.mean(wet ** 2)) * 10 ** (-28 / 20)
  added = wet * wet_gain
  subtle_wet = shape_harmonic_band(process(harmonic_input_band(mild), WET))
  subtle_gain = np.sqrt(np.mean(mild ** 2) / np.mean(subtle_wet ** 2)) * 10 ** (-34 / 20)
  variants = [('original', 'Native 48 kHz PCM', original), ('gentle', 'Gentle EQ', mild), ('stronger', 'Stronger EQ', strong), ('harmonics', 'Gentle EQ + synthetic harmonics', mild + added), ('subtle', 'Gentle EQ + subtler harmonics', mild + subtle_wet * subtle_gain)]
  measurements = {key: loudness(samples) for key, _, samples in variants}
  target = min(-24., min(values['input_i'] - values['input_tp'] - 3 for values in measurements.values()))
  metadata = {'source': 'phone-music-camera-01, native 48 kHz real microphone capture', 'source_sha256': hashlib.sha256(SOURCE.read_bytes()).hexdigest(), 'capture_interval_seconds': [offset + 2, offset + 47], 'working_gain': work_gain, 'gentle_eq_filter': MILD, 'stronger_eq_filter': STRONG, 'synthetic_wet_filter': WET, 'wet_input_fir': '513-tap centered FIR, pass 2.5–4.5 kHz with 1.8–2.5 and 4.5–5.5 kHz transitions', 'wet_fir': '513-tap centered Hamming-windowed FIR, transitions 5.5–7 and 12–14.5 kHz', 'wet_gain': float(wet_gain), 'subtle_wet_gain': float(subtle_gain), 'subtle_wet_rms_relative_to_gentle_db': -34, 'added_wet_rms_relative_to_gentle_db': -28, 'loudness_target_lufs': target, 'normalization': 'Constant gain only, chosen from measured integrated loudness and true peak. No limiter or compressor.', 'variants': [], 'listener_feedback': 'User preferred both EQs for reducing boomy bass; prior synthetic version was cool but airy/shallow and made vocals sound too high-pitched. Preserve EQ bass cleanup and reduce synthetic effect.', 'scope': 'Offline preference experiment, not a microphone calibration or verified improvement. Harmonic variant deliberately invents content. No denoising; firmware unchanged.'}
  clips = {}
  normalized = []
  for key, title, samples in variants:
    gain = 10 ** ((target - measurements[key]['input_i']) / 20)
    signal = samples * gain
    assert abs(signal).max() < .9
    audio = wav_bytes(signal)
    (ROOT / (key + '.wav')).write_bytes(audio)
    decoded = np.frombuffer(audio[44:], dtype='<i2').astype(float) / 32768
    verified = loudness(decoded)
    assert abs(verified['input_i'] - target) < .15
    assert verified['input_tp'] <= -2.8
    row = {'name': key, 'title': title, 'playback_gain_db': float(20 * np.log10(gain)), 'integrated_lufs': verified['input_i'], 'true_peak_dbfs': verified['input_tp'], 'rms_dbfs': float(20 * np.log10(np.sqrt(np.mean(signal ** 2))))}
    metadata['variants'].append(row)
    clips[key] = {'title': title, 'uri': 'data:audio/wav;base64,' + base64.b64encode(audio).decode()}
    normalized.append((title, signal))
  impulse = np.zeros(RATE)
  impulse[0] = 1
  figure, axes = plt.subplots(2, 1, figsize=(12, 8), constrained_layout=True)
  for filters, label in [(MILD, 'Gentle EQ'), (STRONG, 'Stronger EQ')]:
    response = abs(np.fft.rfft(process(impulse, filters)))
    frequencies = np.fft.rfftfreq(len(impulse), 1 / RATE)
    axes[0].plot(frequencies / 1000, 20 * np.log10(np.maximum(response, 1e-20)), label=label)
  axes[0].set(xlabel='Frequency (kHz)', ylabel='Filter gain (dB)', xlim=(0, 20), title='EQ curves before loudness matching; no measured acoustic inverse')
  axes[0].legend()
  for title, signal in normalized:
    frequencies, power = spectrum(signal)
    axes[1].plot(frequencies / 1000, 10 * np.log10(np.maximum(power, 1e-20)), label=title)
  axes[1].set(xlabel='Frequency (kHz)', ylabel='PSD (dBFS/Hz)', xlim=(0, 20), title='Actual listening files after loudness matching')
  axes[1].legend()
  figure.savefig(ROOT / 'eq-and-spectra.png', dpi=150)
  plt.close(figure)
  # Verify that the nonlinear branch actually produces harmonics on a clean tone.
  time = np.arange(RATE * 2) / RATE
  probe = .1 * np.sin(2 * np.pi * 3000 * time)
  generated = shape_harmonic_band(process(harmonic_input_band(probe), WET))[RATE:RATE + RATE // 2]
  probe_spectrum = abs(np.fft.rfft(generated * np.hanning(len(generated))))
  bins = np.fft.rfftfreq(len(generated), 1 / RATE)
  third = float(probe_spectrum[abs(bins - 9000) < 3].max())
  adjacent = float(probe_spectrum[(bins > 9100) & (bins < 9200)].max())
  metadata['synthetic_probe_3khz_to_9khz_above_adjacent_db'] = float(20 * np.log10(third / max(adjacent, 1e-20)))
  assert metadata['synthetic_probe_3khz_to_9khz_above_adjacent_db'] > 30
  (ROOT / 'metrics.json').write_text(json.dumps(metadata, indent=2) + '\n')
  page = '''<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Mic clarity: EQ and harmonic experiment</title><style>body{font:17px/1.5 system-ui;max-width:1150px;margin:25px auto;padding:0 20px}button{font:inherit;padding:12px;margin:5px;border:1px solid #888;border-radius:7px;cursor:pointer}button[aria-pressed=true]{background:#184b77;color:white}audio,img{width:100%}.note{background:#eee;padding:14px}</style><h1>Clarity experiment: the better iPhone recording</h1><p>One actual comma four microphone recording, five offline listening versions. Click a version to start listening or switch at the same position. Nothing auto-plays.</p><div id="buttons"></div><h2 id="playing">Native 48 kHz PCM</h2><audio id="player" controls preload="none"></audio><p id="error" role="status"></p><button id="restart">Restart this version</button><div class="note"><strong>Native 48 kHz PCM:</strong> playback gain only.<br><strong>Gentle EQ:</strong> less low-frequency weight, broad boosts near 3.2 and 7.5 kHz.<br><strong>Stronger EQ:</strong> more of the same, to make the tradeoff audible.<br><strong>Gentle EQ + synthetic harmonics:</strong> the previous brighter version, with invented upper-band content.<br><strong>Gentle EQ + subtler harmonics:</strong> keeps the same bass cleanup, reduces the added layer by 6 dB in RMS, and derives it from the recorded 2.5–4.5 kHz band. Both synthetic layers are tapered mainly into 7–12 kHz; neither recovers the missing original.</div><p>These are taste experiments, not calibrated microphone correction. EQ also boosts existing noise. Harmonic synthesis can add harshness, intermodulation and hiss; a preference does not establish higher fidelity. No source/reference music is mixed into these clips. No firmware change or new recording was made.</p>'''
  page += f'<p>All clips are matched to {target:.2f} LUFS using constant gain only, with at least approximately 3 dB true-peak headroom. No limiting, compression or denoising. The original raw recording remains unchanged.</p>'
  page += '<img alt="Actual EQ curves and output spectra" src="data:image/png;base64,' + base64.b64encode((ROOT / 'eq-and-spectra.png').read_bytes()).decode() + '">'
  page += '<p>Methods: <a href="https://www.w3.org/TR/audio-eq-cookbook/">RBJ/W3C EQ cookbook</a>; <a href="https://ffmpeg.org/ffmpeg-filters.html#aexciter">FFmpeg harmonic exciter</a>. All playable audio and plots are embedded; these reference links are optional.</p>'
  page += '<script>const clips=' + json.dumps(clips) + ''';const player=document.getElementById('player');let active='original';let generation=0;player.src=clips[active].uri;
function choose(key,restart=false){const stamp=++generation;const position=restart?0:(player.currentTime||0);player.pause();active=key;document.getElementById('playing').textContent=clips[key].title;document.getElementById('error').textContent='';document.querySelectorAll('[data-clip]').forEach(button=>button.setAttribute('aria-pressed',String(button.dataset.clip===key)));player.src=clips[key].uri;player.addEventListener('loadedmetadata',()=>{if(stamp!==generation)return;player.currentTime=Math.min(position,Math.max(0,player.duration-.05));player.play().catch(error=>document.getElementById('error').textContent=error.message);},{once:true});player.load();}
for(const [key,clip] of Object.entries(clips)){const button=document.createElement('button');button.textContent=clip.title;button.dataset.clip=key;button.setAttribute('aria-pressed',String(key===active));button.onclick=()=>choose(key);document.getElementById('buttons').appendChild(button);}document.getElementById('restart').onclick=()=>choose(active,true);</script>'''
  (ROOT / 'index.html').write_text(page)
  print(json.dumps(metadata, indent=2))


if __name__ == '__main__':
  main()
