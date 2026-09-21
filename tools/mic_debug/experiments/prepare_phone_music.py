import ctypes
import json
import subprocess
import wave
from pathlib import Path

import numpy as np

root = Path('/home/batman/tmp/c4-mic-results/phone-music-01')
root.mkdir(exist_ok=True)
rate = 48000
source = Path('/home/batman/tmp/c4-mic-session/reference-audio/Rgrt_8mXrK8.webm')
raw = subprocess.check_output(['/usr/bin/ffmpeg', '-v', 'error', '-ss', '47.0127', '-i', str(source), '-t', '45', '-ar', str(rate), '-ac', '2', '-f', 'f32le', '-'])
samples = np.frombuffer(raw, dtype='<f4').reshape(-1, 2).astype(float)
gain = 0.2 / np.max(np.abs(samples))
samples *= gain
# Two seconds of leading silence allow the remotely selected file to start cleanly.
playback = np.concatenate([np.zeros((rate * 2, 2)), samples, np.zeros((rate, 2))])
with wave.open(str(root / 'phone-music.wav'), 'wb') as recording:
  recording.setparams((2, 2, rate, 0, 'NONE', 'not compressed'))
  recording.writeframes(np.rint(playback * 32767).astype('<i2').tobytes())
(root / 'stimulus.json').write_text(json.dumps({'source': 'Existing YouTube studio radio edit reference Rgrt_8mXrK8, not the Spotify stream', 'source_offset_seconds': 47.0127, 'music_seconds': 45, 'leading_silence_seconds': 2, 'trailing_silence_seconds': 1, 'rate': rate, 'channels': 2, 'linear_gain': gain, 'peak': 0.2, 'physical_phone_volume': 'unchanged user setting, approximately 60 percent'}, indent=2)+'\n')
flite = ctypes.CDLL('/usr/lib/x86_64-linux-gnu/libflite.so.1', mode=ctypes.RTLD_GLOBAL)
voices = ctypes.CDLL('/usr/lib/x86_64-linux-gnu/libflite_cmu_us_slt.so.1')
flite.flite_init()
voices.register_cmu_us_slt.argtypes = [ctypes.c_char_p]
voices.register_cmu_us_slt.restype = ctypes.c_void_p
voice = voices.register_cmu_us_slt(None)
flite.flite_text_to_speech.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_char_p]
flite.flite_text_to_speech.restype = ctypes.c_float
for name, message in [('start', 'Music microphone test. The phone will play forty five seconds of Get Lucky. Keep its position and volume unchanged. Recording in three, two, one.'), ('done', 'Music recording finished. Thank you.')]:
  path = root / (name+'.wav')
  flite.flite_text_to_speech(message.encode(), voice, str(path).encode())
  with wave.open(str(path), 'rb') as recording:
    voice_rate = recording.getframerate()
    speech = np.frombuffer(recording.readframes(recording.getnframes()), dtype='<i2').astype(float)
  speech *= 32767 / max(np.max(np.abs(speech)), 1)
  with wave.open(str(path), 'wb') as recording:
    recording.setparams((1, 2, voice_rate, 0, 'NONE', 'not compressed'))
    recording.writeframes(np.rint(speech).astype('<i2').tobytes())
