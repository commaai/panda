import subprocess
from pathlib import Path

ROOT=Path('/home/batman/tmp/c4-mic-results/phone-repeat-01')
HOST='comma@192.168.63.47'
remote='''import json
import subprocess
import time
from pathlib import Path

root=Path('/data/mic-lab/phone-sweep-01')
print('Waiting for the iPhone page to be enabled in the foreground.', flush=True)
deadline=time.monotonic()+180
while time.monotonic()<deadline:
  clients=[json.loads(path.read_text()) for path in (root/'clients').glob('*.json')]
  ready=[item for item in clients if item.get('armed') and item.get('visible') and item.get('audio_ready')
         and 'iPhone' in item.get('user_agent','') and time.monotonic()-item.get('received_monotonic',0)<4]
  if len(ready)==1:
    subprocess.run(['python3','/data/mic-lab/phone-sweep-01/record.py','/data/mic-lab/phone-repeat-01'],check=True)
    break
  time.sleep(0.5)
else:
  raise SystemExit('No recording started: phone did not reconnect in the foreground.')
'''
with (ROOT/'run.log').open('w') as log:
  process=subprocess.Popen(['ssh',HOST,"bash -lc 'source /etc/profile && cd /data/openpilot && python3 -'"],stdin=subprocess.PIPE,
                           stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
  process.stdin.write(remote)
  process.stdin.close()
  for line in process.stdout:
    print(line,end='',flush=True)
    log.write(line)
    log.flush()
  status=process.wait()
  if status:
    raise SystemExit(status)
for name in ['captured.wav','captured.npz','captured.json','phone_before.json','phone_started.json','phone_after.json','playback-command.json']:
  subprocess.run(['scp','-q',HOST+':/data/mic-lab/phone-repeat-01/'+name,str(ROOT/name)],check=True)
subprocess.run(['scp','-q',HOST+':/data/mic-lab/phone-sweep-01/events.jsonl',str(ROOT/'browser-events.jsonl')],check=True)
