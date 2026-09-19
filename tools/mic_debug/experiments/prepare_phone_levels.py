import ctypes
import json
import wave
from pathlib import Path

import numpy as np

root = Path('/home/batman/tmp/c4-mic-results/phone-levels-01')
root.mkdir(exist_ok=True)
rate = 48000
tones = [1000, 8000, 14000, 16000, 17000, 18000, 19000, 20000]
levels = [0.025, 0.1, 0.2]
samples = np.zeros(rate * 38)
schedule = []
for level_index, peak in enumerate(levels):
  for tone_index, frequency in enumerate(tones):
    start = 2 + (level_index * len(tones) + tone_index) * 1.5
    tone_time = np.arange(rate) / rate
    envelope = np.minimum(np.minimum(tone_time / 0.05, (1 - tone_time) / 0.05), 1)
    samples[round(start * rate):round((start + 1) * rate)] = peak * envelope * np.sin(2 * np.pi * frequency * tone_time)
    schedule.append({'start': start, 'duration': 1, 'frequency_hz': frequency, 'peak': peak})
with wave.open(str(root / 'phone-levels.wav'), 'wb') as recording:
  recording.setparams((1, 2, rate, 0, 'NONE', 'not compressed'))
  recording.writeframes(np.rint(samples * 32767).astype('<i2').tobytes())
(root / 'stimulus.json').write_text(json.dumps({'rate': rate, 'duration': 38, 'schedule': schedule, 'physical_phone_volume': 'user reported about 60 percent; not changed remotely'}, indent=2) + '\n')
flite = ctypes.CDLL('/usr/lib/x86_64-linux-gnu/libflite.so.1', mode=ctypes.RTLD_GLOBAL)
voices = ctypes.CDLL('/usr/lib/x86_64-linux-gnu/libflite_cmu_us_slt.so.1')
flite.flite_init()
voices.register_cmu_us_slt.argtypes = [ctypes.c_char_p]
voices.register_cmu_us_slt.restype = ctypes.c_void_p
voice = voices.register_cmu_us_slt(None)
flite.flite_text_to_speech.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_char_p]
flite.flite_text_to_speech.restype = ctypes.c_float
messages = [('start', 'Microphone level test. Leave the phone still at its current volume. Three sets of tones will play, from quiet to moderately louder. Recording starts in three, two, one.'),
            ('done', 'Microphone level recording finished. Thank you.'),
            ('refresh', 'Please refresh the phone speaker page and tap Enable speaker once. This lets me choose the next test sounds automatically.')]
for name, message in messages:
  path = root / (name + '.wav')
  flite.flite_text_to_speech(message.encode(), voice, str(path).encode())
  with wave.open(str(path), 'rb') as recording:
    voice_rate = recording.getframerate()
    speech = np.frombuffer(recording.readframes(recording.getnframes()), dtype='<i2').astype(float)
  speech *= 32767 / max(np.max(np.abs(speech)), 1)
  with wave.open(str(path), 'wb') as recording:
    recording.setparams((1, 2, voice_rate, 0, 'NONE', 'not compressed'))
    recording.writeframes(np.rint(speech).astype('<i2').tobytes())
