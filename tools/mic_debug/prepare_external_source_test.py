import ctypes
import json
import wave
from pathlib import Path

import numpy as np

root = Path('/home/batman/tmp/c4-mic-results/headphone-source-01')
root.mkdir(exist_ok=True)
rate = 48000
frequencies = [1000, 8000, 14000, 16000, 17000, 18000, 19000, 20000, 21000, 22000]
samples = np.zeros((rate * 35, 2))
schedule = []
for index, frequency in enumerate(frequencies):
  start = 2 + index * 3
  time = np.arange(rate * 2) / rate
  envelope = np.minimum(np.minimum(time / .1, (2 - time) / .1), 1)
  tone = .1 * envelope * np.sin(2 * np.pi * frequency * time)
  samples[start * rate:(start + 2) * rate] = tone[:, None]
  schedule.append({'frequency_hz': frequency, 'start_seconds': start, 'duration_seconds': 2, 'peak': .1})
with wave.open(str(root / 'headphone-tones.wav'), 'wb') as recording:
  recording.setparams((2, 2, rate, 0, 'NONE', 'not compressed'))
  recording.writeframes(np.rint(samples * 32767).astype('<i2').tobytes())
(root / 'stimulus.json').write_text(json.dumps({'sample_rate': rate, 'duration_seconds': 35, 'channels': 'identical left and right', 'schedule': schedule, 'status': 'prepared only; awaiting user placement/readiness, no playback or capture yet'}, indent=2) + '\n')
flite = ctypes.CDLL('/usr/lib/x86_64-linux-gnu/libflite.so.1', mode=ctypes.RTLD_GLOBAL)
voices = ctypes.CDLL('/usr/lib/x86_64-linux-gnu/libflite_cmu_us_slt.so.1')
flite.flite_init()
voices.register_cmu_us_slt.argtypes = [ctypes.c_char_p]
voices.register_cmu_us_slt.restype = ctypes.c_void_p
voice = voices.register_cmu_us_slt(None)
flite.flite_text_to_speech.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_char_p]
flite.flite_text_to_speech.restype = ctypes.c_float
for name, message in [('start', 'Independent speaker test. Keep the headphones off your ears, with the open ear cup facing the comma four. The headphones will play tones while I record the microphone. Three, two, one.'), ('done', 'Independent speaker recording finished. No more sounds will play.')]:
  path = root / (name + '.wav')
  flite.flite_text_to_speech(message.encode(), voice, str(path).encode())
  with wave.open(str(path)) as recording:
    voice_rate = recording.getframerate()
    speech = np.frombuffer(recording.readframes(recording.getnframes()), dtype='<i2').astype(float)
  speech *= 32767 / max(abs(speech).max(), 1)
  with wave.open(str(path), 'wb') as recording:
    recording.setparams((1, 2, voice_rate, 0, 'NONE', 'not compressed'))
    recording.writeframes(np.rint(speech).astype('<i2').tobytes())
print('Prepared stimulus and cues only; did not play or record.')
