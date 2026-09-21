import subprocess
from pathlib import Path

ROOT = Path('/home/batman/tmp/c4-mic-results')
PYTHON = '/home/batman/openpilot/.venv/bin/python'
for name in ('headphone-stationary-tones-01', 'headphone-stationary-music-01'):
  print('Starting ' + name, flush=True)
  subprocess.run([PYTHON, str(ROOT / name / 'run.py')], check=True)
print('Both recordings complete. No more playback scheduled.', flush=True)
