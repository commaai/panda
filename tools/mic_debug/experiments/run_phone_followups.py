import subprocess
from pathlib import Path

root = Path('/home/batman/tmp/c4-mic-results')
remote = '''import json
import subprocess
import time
from pathlib import Path

root = Path('/data/mic-lab/phone-sweep-01')
deadline = time.monotonic() + 300
while time.monotonic() < deadline:
  clients = [json.loads(path.read_text()) for path in (root / 'clients').glob('*.json')]
  ready = [item for item in clients if item.get('armed') and item.get('visible') and item.get('audio_ready') and item.get('protocol', 0) >= 2 and time.monotonic()-item.get('received_monotonic', 0)<4]
  if len(ready) == 1:
    print('Updated phone connected. Starting music, then level test, each with spoken countdown.', flush=True)
    break
  time.sleep(0.5)
else:
  raise SystemExit('No recording started: updated phone page not ready.')
for name, source in [('phone-music-01', 'phone-music.wav'), ('phone-levels-01', 'phone-levels.wav')]:
  subprocess.run(['python3', str(root / 'record.py'), '/data/mic-lab/' + name, '--source', source], check=True)
'''
with (root / 'phone-followups.log').open('w') as log:
  process = subprocess.Popen(['ssh', 'comma@192.168.63.47', "bash -lc 'source /etc/profile && cd /data/openpilot && python3 -'"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
  process.stdin.write(remote)
  process.stdin.close()
  for line in process.stdout:
    print(line, end='', flush=True)
    log.write(line)
    log.flush()
  status = process.wait()
  if status:
    raise SystemExit(status)
for name in ['phone-music-01', 'phone-levels-01']:
  for filename in ['captured.wav', 'captured.npz', 'captured.json', 'phone_before.json', 'phone_started.json', 'phone_after.json', 'playback-command.json']:
    subprocess.run(['scp', '-q', 'comma@192.168.63.47:/data/mic-lab/' + name + '/' + filename, str(root / name / filename)], check=True)
