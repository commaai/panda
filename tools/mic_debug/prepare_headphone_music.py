import ctypes
import hashlib
import json
import shutil
import wave
from pathlib import Path

import numpy as np

SESSION = Path('/home/batman/tmp/c4-mic-session')
ROOT = Path('/home/batman/tmp/c4-mic-results/headphone-music-01')
SOURCE = Path('/home/batman/tmp/c4-mic-results/phone-music-camera-01/phone-music.wav')
ROOT.mkdir(exist_ok=True)
if (ROOT / 'captured.npz').exists():
  raise RuntimeError('Recording already exists')
shutil.copy2(SOURCE, ROOT / 'headphone-music.wav')
(ROOT / 'stimulus.json').write_text(json.dumps({'source': 'Same stereo music file used for phone-music-camera-01', 'sha256': hashlib.sha256(SOURCE.read_bytes()).hexdigest(), 'music_seconds': 45, 'duration_seconds': 48, 'leading_silence_seconds': 2, 'peak': 0.2, 'sample_rate': 48000, 'channels': 2}, indent=2) + '\n')
remote = (SESSION / 'headphone_capture_remote.py').read_text().replace('headphone-source-01', 'headphone-music-01').replace("'duration': 45", "'duration': 58").replace("'45', '--rate'", "'58', '--rate'").replace('timeout=65', 'timeout=78').replace('external headphone tones.', 'external headphone music.').replace('headphones playing spaced tones from 1 to 22 kHz.', 'headphones playing the same 45-second music excerpt.').replace('Independent headphone source test', 'AirPods music microphone test')
(ROOT / 'record.py').write_text(remote)
runner = (SESSION / 'run_headphone_capture.py').read_text().replace('headphone-source-01', 'headphone-music-01').replace('headphone-tones.wav', 'headphone-music.wav').replace("'stimulus_peak': 0.1", "'stimulus_peak': 0.2").replace('+ 110', '+ 130')
(SESSION / 'run_headphone_music.py').write_text(runner)
flite = ctypes.CDLL('/usr/lib/x86_64-linux-gnu/libflite.so.1', mode=ctypes.RTLD_GLOBAL)
voices = ctypes.CDLL('/usr/lib/x86_64-linux-gnu/libflite_cmu_us_slt.so.1')
flite.flite_init()
voices.register_cmu_us_slt.argtypes = [ctypes.c_char_p]
voices.register_cmu_us_slt.restype = ctypes.c_void_p
voice = voices.register_cmu_us_slt(None)
flite.flite_text_to_speech.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_char_p]
flite.flite_text_to_speech.restype = ctypes.c_float
for name, message in [('start', 'AirPods music test. Keep the headphones off your ears and in the same position by the comma four. The headphones will play forty five seconds of Get Lucky while I record. Three, two, one.'), ('done', 'AirPods music recording finished. Thank you.')]:
  path = ROOT / (name + '.wav')
  flite.flite_text_to_speech(message.encode(), voice, str(path).encode())
  with wave.open(str(path)) as recording:
    rate = recording.getframerate()
    samples = np.frombuffer(recording.readframes(recording.getnframes()), dtype='<i2').astype(float)
  samples *= 32767 / max(abs(samples).max(), 1)
  with wave.open(str(path), 'wb') as recording:
    recording.setparams((1, 2, rate, 0, 'NONE', 'not compressed'))
    recording.writeframes(np.rint(samples).astype('<i2').tobytes())
print('Prepared byte-identical music source and spoken cues.')
