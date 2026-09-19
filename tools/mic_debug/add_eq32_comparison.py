"""Expose the existing 48 kHz / 32 kbps processed audition beside the proposal."""
import hashlib
import html
import json
import shutil
import wave
from pathlib import Path

RESULTS = Path('/home/batman/tmp/c4-mic-results')
ROOT = RESULTS / 'clarity-comparison-01'
KEY = 'processed48_aac32'
TITLE = '5. Halfway EQ + denoise — 48 kHz AAC / 32 kbps'
DESCRIPTION = 'The same halfway EQ + light denoise target encoded at 48 kHz with AAC at 32 kbps. Compare with 3 for compression loss, or 4 for the bitrate tradeoff. Same audio as the earlier Candidate G.'
SOURCE = RESULTS / 'blind-compression-01/private/aac_32-playback.wav'
with wave.open(str(SOURCE)) as audio:
  assert audio.getframerate() == 48000 and audio.getnframes() == 45 * 48000
page = (ROOT / 'index.html').read_text()
start = page.index('const embeddedAudio=') + len('const embeddedAudio=')
end = page.index(';for(const key of Object.keys(embeddedAudio))', start)
data = json.loads(page[start:end])
assert 'uri' in data['blind_g']
encoded = data['blind_g']['uri'].split(',', 1)[1]
# The existing candidate is reused as an alias, adding no extra embedded audio.
data[KEY] = {'title': TITLE, 'aliasOf': 'blind_g'}
page = page[:start] + json.dumps(data) + page[end:]
start = page.index('const clips=') + len('const clips=')
end = page.index(';const player=', start)
clips = json.loads(page[start:end])
clips[KEY] = {'title': TITLE}
page = page[:start] + json.dumps(clips) + page[end:]
old = "['logged','native48_aac32','reference_half_denoise','proposed48_aac64'].includes(key)"
new = "['logged','native48_aac32','reference_half_denoise','proposed48_aac64','processed48_aac32'].includes(key)"
assert old in page or new in page
page = page.replace(old, new)
legend_start = page.index('<div class="note" id="clip-legend">')
legend_end = page.index('</div>', legend_start)
if html.escape(TITLE) + ':</strong>' not in page[legend_start:legend_end]:
  insertion = page.index('<details>', legend_start)
  page = page[:insertion] + '<p><strong>' + html.escape(TITLE) + ':</strong> ' + html.escape(DESCRIPTION) + '</p>' + page[insertion:]
page = page.replace('Four-way logging comparison', 'Logging and compression comparison')
page = page.replace('Compare 1 → 2 for sample rate, and 3 → 4 for the proposed compression.', 'Compare 1 → 2 for sample rate; compare 3 → 4 or 5 for compression, and 4 → 5 for bitrate.')
assert len(page.encode()) < 100 * 1024 * 1024
pending = ROOT / 'index.pending.html'
pending.write_text(page)
pending.replace(ROOT / 'index.html')
output = RESULTS / 'four-way-01/processed48_aac32.wav'
shutil.copy2(SOURCE, output)
key = json.loads((RESULTS / 'blind-compression-01/private/answer-key.json').read_text())['candidates']['G']
assert key['identifier'] == 'aac_32'
record = {'title': TITLE, 'sample_rate': 48000, 'target_bitrate': 32000, 'duration_seconds': 45, 'integrated_lufs': key['integrated_lufs'], 'true_peak_dbfs': key['true_peak_dbfs'], 'playback_sha256': hashlib.sha256(SOURCE.read_bytes()).hexdigest(), 'reused_candidate': 'G', 'description': DESCRIPTION}
(RESULTS / 'four-way-01/processed48-aac32.json').write_text(json.dumps(record, indent=2) + '\n')
print(json.dumps(record, indent=2))
