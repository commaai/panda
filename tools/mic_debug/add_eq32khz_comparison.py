"""Audition 32 kHz AAC at 32 kbps after the preferred 48 kHz processing."""
import base64
import hashlib
import html
import json
import re
import subprocess
import wave
from pathlib import Path

import numpy as np

from make_clarity_comparison import loudness, wav_bytes

RESULTS = Path('/home/batman/tmp/c4-mic-results')
PAGE_ROOT = RESULTS / 'clarity-comparison-01'
ROOT = RESULTS / 'four-way-01'
SOURCE = PAGE_ROOT / 'reference_half_denoise.wav'
ENCODED = ROOT / 'processed32-aac32.m4a'
PLAYBACK = ROOT / 'processed32_aac32.wav'
KEY = 'processed32_aac32'
TITLE = '7. Halfway EQ + denoise — 32 kHz AAC / 32 kbps'
DESCRIPTION = 'The same preferred 48 kHz halfway EQ + light denoise recording, downsampled to 32 kHz before AAC encoding at 32 kbps. Compare 5 versus 7 for sample rate at the same bitrate, or 3 versus 7 for overall loss. Decoded to 48 kHz PCM only for consistent playback; this does not restore discarded frequencies.'
FFMPEG = '/usr/bin/ffmpeg'
RATE = 48000
TARGET = -25.70
subprocess.run([FFMPEG, '-v', 'error', '-y', '-i', str(SOURCE), '-map_metadata', '-1', '-ar', '32000', '-ac', '1', '-c:a', 'aac', '-b:a', '32000', str(ENCODED)], check=True)
probe = json.loads(subprocess.check_output(['/usr/bin/ffprobe', '-v', 'error', '-select_streams', 'a:0', '-show_entries', 'stream=codec_name,sample_rate,channels,bit_rate,duration', '-of', 'json', str(ENCODED)]))['streams'][0]
assert probe['codec_name'] == 'aac' and int(probe['sample_rate']) == 32000 and probe['channels'] == 1
raw = subprocess.check_output([FFMPEG, '-v', 'error', '-i', str(ENCODED), '-f', 'f64le', '-ar', str(RATE), '-ac', '1', '-'])
decoded = np.frombuffer(raw, dtype='<f8')
assert len(decoded) >= 45 * RATE
decoded = decoded[:45 * RATE]
with wave.open(str(SOURCE)) as audio:
  source = np.frombuffer(audio.readframes(audio.getnframes()), dtype='<i2').astype(float) / 32768
start, length = RATE, 4 * RATE
size = 1 << (2 * length - 1).bit_length()
correlation = np.fft.irfft(np.fft.rfft(decoded[start:start+length], size) * np.conj(np.fft.rfft(source[start:start+length], size)), size)
peak = int(np.argmax(correlation))
lag = peak if peak < size // 2 else peak - size
assert abs(lag) <= 1
before = loudness(decoded)
gain_db = TARGET - before['input_i']
signal = decoded * 10 ** (gain_db / 20)
assert abs(signal).max() < .95
payload = wav_bytes(signal)
PLAYBACK.write_bytes(payload)
verified = loudness(np.frombuffer(payload[44:], dtype='<i2').astype(float) / 32768)
assert abs(verified['input_i'] - TARGET) < .1 and verified['input_tp'] < -2.8
record = {'title': TITLE, 'description': DESCRIPTION, 'codec': probe, 'duration_seconds': 45, 'container_bytes': ENCODED.stat().st_size, 'container_MB_per_minute': ENCODED.stat().st_size / 45 * 60 / 1e6, 'playback_sample_rate': RATE, 'source_sha256': hashlib.sha256(SOURCE.read_bytes()).hexdigest(), 'playback_sha256': hashlib.sha256(payload).hexdigest(), 'integrated_lufs': verified['input_i'], 'true_peak_dbfs': verified['input_tp'], 'playback_gain_db': gain_db, 'alignment_lag_samples': lag}
(ROOT / 'processed32-aac32.json').write_text(json.dumps(record, indent=2) + '\n')
page = (PAGE_ROOT / 'index.html').read_text()
start = page.index('const embeddedAudio=') + len('const embeddedAudio=')
end = page.index(';for(const key of Object.keys(embeddedAudio))', start)
data = json.loads(page[start:end])
data[KEY] = {'title': TITLE, 'uri': 'data:audio/wav;base64,' + base64.b64encode(payload).decode()}
page = page[:start] + json.dumps(data) + page[end:]
start = page.index('const clips=') + len('const clips=')
end = page.index(';const player=', start)
clips = json.loads(page[start:end])
clips[KEY] = {'title': TITLE}
page = page[:start] + json.dumps(clips) + page[end:]
match = re.search(r'''\[(?:'|")logged(?:'|")[^\]]+\]\.includes\(key\)''', page)
assert match
primary = json.loads(match.group(0).split('.includes')[0].replace("'", '"'))
if KEY not in primary:
  primary.append(KEY)
page = page[:match.start()] + json.dumps(primary) + '.includes(key)' + page[match.end():]
legend_start = page.index('<div class="note" id="clip-legend">')
legend_end = page.index('</div>', legend_start)
if html.escape(TITLE) + ':</strong>' not in page[legend_start:legend_end]:
  insertion = page.index('<details>', legend_start)
  page = page[:insertion] + '<p><strong>' + html.escape(TITLE) + ':</strong> ' + html.escape(DESCRIPTION) + '</p>' + page[insertion:]
page = page.replace('Buttons 4/5/6 use 64/32/48 kbps respectively.', 'Buttons 4/5/6 use 64/32/48 kbps respectively. Compare 5 versus 7 for 48 versus 32 kHz at the same 32 kbps bitrate.')
assert len(page.encode()) < 100 * 1024 * 1024
pending = PAGE_ROOT / 'index.pending.html'
pending.write_text(page)
pending.replace(PAGE_ROOT / 'index.html')
print(json.dumps(record, indent=2))
