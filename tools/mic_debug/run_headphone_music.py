"""Run only after placement/readiness confirmation; no global volume changes."""
import json
import selectors
import subprocess
import time
from pathlib import Path

ROOT = Path('/home/batman/tmp/c4-mic-results/headphone-music-01')
REMOTE = '/data/mic-lab/headphone-music-01'
HOST = 'comma@192.168.63.47'


def main():
  if (ROOT / 'captured.npz').exists():
    raise RuntimeError('Refusing to overwrite a capture')
  sinks = json.loads(subprocess.check_output(['pactl', '--format=json', 'list', 'sinks']))
  selected = [sink for sink in sinks if 'AirPods' in sink.get('description', '')]
  if len(selected) != 1 or selected[0]['mute']:
    raise RuntimeError('Expected one unmuted AirPods output')
  sink = selected[0]
  metadata = {'source': 'USB AirPods Max', 'sample_specification': sink['sample_specification'],
              'volume': sink['volume'], 'stimulus_peak': 0.2,
              'placement': 'Open earcup close to USB corner; device flat, no acoustic seal',
              'calibrated_source': False, 'started_unix': time.time()}
  (ROOT / 'playback.json').write_text(json.dumps(metadata, indent=2) + '\n')
  process = subprocess.Popen(['ssh', HOST,
                             "bash -lc 'source /etc/profile && cd /data/openpilot && python3 -u " + REMOTE + "/record.py'"],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
  player = None
  pending = b''
  selector = selectors.DefaultSelector()
  selector.register(process.stdout, selectors.EVENT_READ)
  deadline = time.monotonic() + 130
  try:
    with (ROOT / 'run.log').open('w') as log:
      while selector.get_map():
        if time.monotonic() > deadline:
          raise TimeoutError('Remote recording timed out')
        for key, _ in selector.select(timeout=1):
          chunk = key.fileobj.read1(65536)
          if not chunk:
            selector.unregister(key.fileobj)
            continue
          pending += chunk
          while b'\n' in pending:
            raw_line, pending = pending.split(b'\n', 1)
            line = raw_line.decode(errors='replace')
            print(line, flush=True)
            log.write(line + '\n')
            log.flush()
            if line.startswith('{') and json.loads(line).get('recording'):
              if player is not None:
                raise RuntimeError('Duplicate capture ready event')
              metadata['play_requested_unix'] = time.time()
              player = subprocess.Popen(['paplay', '--device=' + sink['name'], str(ROOT / 'headphone-music.wav')])
      if process.wait(timeout=5):
        raise RuntimeError('Remote helper failed')
      if player is None or player.wait(timeout=5):
        raise RuntimeError('Headphone playback failed')
      metadata['player_exit_code'] = 0
  finally:
    selector.close()
    for child in (player, process):
      if child is not None and child.poll() is None:
        child.terminate()
        child.wait(timeout=5)
    (ROOT / 'playback.json').write_text(json.dumps(metadata, indent=2) + '\n')
  for name in ('captured.wav', 'captured.npz', 'captured.json'):
    subprocess.run(['scp', '-q', HOST + ':' + REMOTE + '/' + name, str(ROOT / name)], check=True)


if __name__ == '__main__':
  main()
