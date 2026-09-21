"""Keep the legend beneath Restart aligned with every embedded listening button."""
import html
import json
from pathlib import Path

RESULTS = Path('/home/batman/tmp/c4-mic-results')
DESCRIPTIONS = {
  'logged': 'The same capture reduced to 16 kHz mono and AAC at 32 kbps, then decoded for playback. Approximates current logged video quality; no old firmware clicks are simulated.',
  'original': 'Native 48 kHz microphone capture with playback gain only.',
  'gentle': 'Hand-tuned EQ: a small bass cut and broad boosts near 3.2 and 7.5 kHz.',
  'gentle_body': 'The same hand-tuned treble boosts with the bass cut removed.',
  'reference_half': 'Half-strength EQ derived from the measured tonal difference against the source music. No denoising or synthesis.',
  'reference_half_denoise': 'The same measured halfway EQ plus light noise-profile suppression above 2–3.5 kHz, capped at 6 dB. Current listening favorite at a slightly lower playback volume; may soften detail. No synthesis.',
  'reference_full': 'Stronger measured correction toward the source music. Can bring out more hiss and room noise. No denoising or synthesis.',
  'stronger': 'Stronger hand-tuned bass reduction and treble boosts, to make the tradeoff audible.',
  'harmonics': 'Archived experiment: gentle EQ plus invented upper-frequency content. Not recovered original detail; synthesis is no longer the selected direction.',
  'subtle': 'Archived experiment: a quieter synthetic layer derived mainly from 2.5–4.5 kHz. The listener reported clipping-like treble harshness despite export headroom.',
  'reference': 'The separate stereo source file played on the iPhone, included only as a listening target. It is not mixed into the microphone versions.',
}

for experiment in ('clarity-comparison-01', 'reference-eq-01'):
  path = RESULTS / experiment / 'index.html'
  page = path.read_text()
  start = page.index('const clips=') + len('const clips=')
  clips = json.loads(page[start:page.index(';const player=', start)])
  marker = '<button id="restart">Restart this version</button>'
  legend_start = page.index(marker) + len(marker)
  assert page[legend_start:].startswith('<div ')
  legend_end = page.index('</div>', legend_start) + len('</div>')
  entries = ''.join('<p><strong>' + html.escape(clip['title']) + ':</strong> ' + html.escape(DESCRIPTIONS[key]) + '</p>' for key, clip in clips.items())
  legend = '<div class="note" id="clip-legend">' + entries + '<p>The measured reference fit includes the iPhone speaker, room, enclosure and microphone. It is not a mic-only calibration.</p></div>'
  page = page[:legend_start] + legend + page[legend_end:]
  path.write_text(page)
  assert all(html.escape(clip['title']) + ':</strong>' in legend for clip in clips.values())
  print(experiment, 'legend entries:', len(clips))
