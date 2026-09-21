import ctypes
import json
import shutil
import subprocess
import wave
from pathlib import Path

import numpy as np

SESSION = Path('/home/batman/tmp/c4-mic-session')
RESULTS = Path('/home/batman/tmp/c4-mic-results')
PAIRS = [('headphone-source-01', 'headphone-stationary-tones-01', 'run_headphone_capture.py', 'analyze_headphone_capture.py'), ('headphone-music-01', 'headphone-stationary-music-01', 'run_headphone_music.py', 'analyze_headphone_music.py')]
flite = ctypes.CDLL('/usr/lib/x86_64-linux-gnu/libflite.so.1', mode=ctypes.RTLD_GLOBAL)
voices = ctypes.CDLL('/usr/lib/x86_64-linux-gnu/libflite_cmu_us_slt.so.1')
flite.flite_init()
voices.register_cmu_us_slt.argtypes = [ctypes.c_char_p]
voices.register_cmu_us_slt.restype = ctypes.c_void_p
voice = voices.register_cmu_us_slt(None)
flite.flite_text_to_speech.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_char_p]
flite.flite_text_to_speech.restype = ctypes.c_float
for old, new, runner_name, analysis_name in PAIRS:
  root = RESULTS / new
  root.mkdir(exist_ok=True)
  if (root / 'captured.npz').exists():
    raise RuntimeError('Refusing to overwrite recording')
  source_name = 'headphone-tones.wav' if 'tones' in new else 'headphone-music.wav'
  shutil.copy2(RESULTS / old / source_name, root / source_name)
  metadata = json.loads((RESULTS / old / 'stimulus.json').read_text())
  metadata.pop('status', None)
  metadata['placement'] = 'User confirmed stationary before the pair; leave unchanged through tones and music.'
  (root / 'stimulus.json').write_text(json.dumps(metadata, indent=2) + '\n')
  remote = (SESSION / 'headphone_capture_remote.py').read_text() if 'tones' in new else (RESULTS / old / 'record.py').read_text()
  (root / 'record.py').write_text(remote.replace(old, new))
  runner = (SESSION / runner_name).read_text().replace(old, new)
  runner = runner.replace("'calibrated_source': False", "'stationary_confirmed_before_pair': True, 'calibrated_source': False")
  (root / 'run.py').write_text(runner)
  analysis = (SESSION / analysis_name).read_text().replace(old, new)
  if 'music' in new:
    old_note = 'The user reported moving the AirPods; whether this happened between tests, during music, or both is not yet established. Placement was not controlled, so the weaker music result cannot be compared with the tone test as a fixed-position measurement.'
    analysis = analysis.replace(old_note, 'User confirmed stationary before this tone-and-music pair and was asked to leave the earcup in place throughout. This is still an uncalibrated source and geometry.')
    analysis = analysis.replace('<strong>Movement reported:</strong>', '<strong>Stationary repeat:</strong>')
  (root / 'analyze.py').write_text(analysis)
  if 'tones' in new:
    messages = [('start', 'Stationary comparison. Keep the headphones off your ears, facing the comma four, and do not move them. First, tones. Music will follow after a second countdown. Recording in three, two, one.'), ('done', 'Tone recording finished. Leave the headphones exactly where they are. Music is next.')]
  else:
    messages = [('start', 'Now the music recording. Keep the headphones in exactly the same position. Forty five seconds of music. Recording in three, two, one.'), ('done', 'The stationary tone and music comparison is finished. You can move the headphones now.')]
  for name, message in messages:
    path = root / (name + '.wav')
    flite.flite_text_to_speech(message.encode(), voice, str(path).encode())
    with wave.open(str(path)) as recording:
      rate = recording.getframerate()
      samples = np.frombuffer(recording.readframes(recording.getnframes()), dtype='<i2').astype(float)
    samples *= 32767 / max(abs(samples).max(), 1)
    with wave.open(str(path), 'wb') as recording:
      recording.setparams((1, 2, rate, 0, 'NONE', 'not compressed'))
      recording.writeframes(np.rint(samples).astype('<i2').tobytes())
  subprocess.run(['ssh', 'comma@192.168.63.47', 'mkdir -p /data/mic-lab/' + new], check=True)
  subprocess.run(['scp', '-q', str(root / 'record.py'), str(root / 'start.wav'), str(root / 'done.wav'), 'comma@192.168.63.47:/data/mic-lab/' + new + '/'], check=True)
print('Prepared stationary pair, no playback yet.')
