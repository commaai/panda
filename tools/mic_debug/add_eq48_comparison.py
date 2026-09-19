"""Add the existing processed 48 kbps AAC audition to the main comparison."""
import hashlib
import html
import json
import re
import shutil
import wave
from pathlib import Path

RESULTS = Path('/home/batman/tmp/c4-mic-results')
ROOT = RESULTS / 'clarity-comparison-01'
KEY = 'processed48_aac48'
TITLE = '6. Halfway EQ + denoise — 48 kHz AAC / 48 kbps'
DESCRIPTION = 'The same halfway reference EQ + light denoise target encoded at 48 kHz with AAC at 48 kbps. Compare with 3 (PCM), 4 (64 kbps), and 5 (32 kbps). Reuses the earlier Candidate D exactly.'
SOURCE = RESULTS / 'blind-compression-01/private/aac_48-playback.wav'
key = json.loads((RESULTS / 'blind-compression-01/private/answer-key.json').read_text())['candidates']['D']
assert key['identifier'] == 'aac_48'
assert hashlib.sha256(SOURCE.read_bytes()).hexdigest() == key['playback_sha256']
with wave.open(str(SOURCE)) as audio:
  assert audio.getframerate() == 48000 and audio.getnframes() == 45 * 48000
page = (ROOT / 'index.html').read_text()
start = page.index('const embeddedAudio=') + len('const embeddedAudio=')
end = page.index(';for(const key of Object.keys(embeddedAudio))', start)
data = json.loads(page[start:end])
assert 'uri' in data['blind_d']
data[KEY] = {'title': TITLE, 'aliasOf': 'blind_d'}
page = page[:start] + json.dumps(data) + page[end:]
start = page.index('const clips=') + len('const clips=')
end = page.index(';const player=', start)
clips = json.loads(page[start:end])
clips[KEY] = {'title': TITLE}
page = page[:start] + json.dumps(clips) + page[end:]
match = re.search(r"\['logged',[^\]]+\]\.includes\(key\)", page)
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
page = page.replace('compare 3 → 4 or 5 for compression, and 4 → 5 for bitrate.', 'compare 3 against 4, 5, or 6 for compression. Buttons 4/5/6 use 64/32/48 kbps respectively.')
pending = ROOT / 'index.pending.html'
pending.write_text(page)
pending.replace(ROOT / 'index.html')
shutil.copy2(SOURCE, RESULTS / 'four-way-01/processed48_aac48.wav')
record = {'title': TITLE, 'sample_rate': 48000, 'target_bitrate': 48000, 'duration_seconds': 45, 'integrated_lufs': key['integrated_lufs'], 'true_peak_dbfs': key['true_peak_dbfs'], 'playback_sha256': key['playback_sha256'], 'reused_candidate': 'D', 'description': DESCRIPTION, 'latest_listener_feedback': 'User preferred 3/4 (processed PCM and 64 kbps), with 5 (32 kbps) not too far behind; requested 48 kbps comparison.'}
(RESULTS / 'four-way-01/processed48-aac48.json').write_text(json.dumps(record, indent=2) + '\n')
print(json.dumps(record, indent=2))
