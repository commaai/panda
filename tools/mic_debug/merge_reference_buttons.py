import base64
import io
import json
import wave
from pathlib import Path

import numpy as np

from make_clarity_comparison import loudness, wav_bytes

ROOT = Path('/home/batman/tmp/c4-mic-results/clarity-comparison-01')
REFERENCE = ROOT.parent / 'reference-eq-01'
page = (ROOT / 'index.html').read_text()
start = page.index('const clips=') + len('const clips=')
end = page.index(';const player=', start)
clips = json.loads(page[start:end])
metadata = json.loads((ROOT / 'metrics.json').read_text())
reference_metadata = json.loads((REFERENCE / 'metrics.json').read_text())
added = {}
measurements = []
for key in ('gentle_body', 'reference_half', 'reference_full'):
  row = next(item for item in reference_metadata['variants'] if item['name'] == key)
  with wave.open(str(REFERENCE / (key + '.wav'))) as audio:
    assert audio.getframerate() == 48000 and audio.getnframes() == 45 * 48000
    samples = np.frombuffer(audio.readframes(audio.getnframes()), dtype='<i2').astype(float) / 32768
  adjustment = metadata['loudness_target_lufs'] - row['integrated_lufs']
  samples *= 10 ** (adjustment / 20)
  assert abs(samples).max() < .9
  payload = wav_bytes(samples)
  with wave.open(io.BytesIO(payload)) as audio:
    decoded = np.frombuffer(audio.readframes(audio.getnframes()), dtype='<i2').astype(float) / 32768
  measured = loudness(decoded)
  assert abs(measured['input_i'] - metadata['loudness_target_lufs']) < .1
  assert measured['input_tp'] < -2.9
  (ROOT / (key + '.wav')).write_bytes(payload)
  added[key] = {'title': row['title'], 'uri': 'data:audio/wav;base64,' + base64.b64encode(payload).decode()}
  measurements.append({'name': key, 'title': row['title'], 'integrated_lufs': measured['input_i'], 'true_peak_dbfs': measured['input_tp'], 'gain_adjustment_from_reference_report_db': adjustment})
merged = {}
for key, clip in clips.items():
  if key not in added:
    merged[key] = clip
  if key == 'gentle':
    merged.update(added)
page = page[:start] + json.dumps(merged) + page[end:]
page = page.replace('One actual comma four microphone recording, five offline listening versions.', 'One actual comma four microphone recording, with logged-quality, EQ, and archived synthesis comparisons.')
if 'id="reference-fit-note"' not in page:
  note = '<div class="note" id="reference-fit-note"><strong>Measured EQ is now on this page:</strong> “Reference fit — halfway” and “Reference fit — closer match” derive from the measured difference against the source music. “Gentle EQ — bass retained” is hand-tuned. All use the same capture and matching playback loudness, with no synthesis. The fit includes the iPhone speaker, room, enclosure and microphone; it is not a mic-only calibration and boosts existing noise too.<p>The synthesis clips remain as archived experiments; the listener reported clipping-like high-frequency harshness in the subtler version. Its exported true peak measured −3 dBFS. Synthesis is not the selected direction.</p></div>'
  page = page.replace('<div id="buttons"></div>', '<div id="buttons"></div>' + note)
  page = page.replace('<script>const clips=', '<h2>Reference-derived EQ measurements</h2><img alt="Measured capture response and reference-derived EQ" src="data:image/png;base64,' + base64.b64encode((REFERENCE / 'reference-fit.png').read_bytes()).decode() + '"><script>const clips=')
(ROOT / 'index.html').write_text(page)
metadata['reference_comparison_variants'] = measurements
metadata['reference_comparison_limitations'] = reference_metadata['limitations']
metadata['latest_listener_feedback'] = 'Subtler harmonics sound like high frequencies clip. User agreed to set synthesis aside and focus on 48 kHz capture and EQ. Existing synth clips retained for documentation.'
(ROOT / 'metrics.json').write_text(json.dumps(metadata, indent=2) + '\n')
print(json.dumps({'buttons': [clip['title'] for clip in merged.values()], 'added_measurements': measurements}, indent=2))
