"""Present the three useful demo comparisons; retain other experiments collapsed."""
import html
import json
from pathlib import Path

ROOT = Path('/home/batman/tmp/c4-mic-results/clarity-comparison-01')
PRIMARY = ['logged', 'original', 'reference_half_denoise']
path = ROOT / 'index.html'
page = path.read_text()
start = page.index('const clips=') + len('const clips=')
clips = json.loads(page[start:page.index(';const player=', start)])
assert all(key in clips for key in PRIMARY)
if 'id="more-experiments"' not in page:
  page = page.replace('<div id="buttons"></div>', '<div id="buttons"></div><details id="more-experiments"><summary>More experiments (' + str(len(clips)-len(PRIMARY)) + ')</summary><div id="experiment-buttons"></div></details>', 1)
  page = page.replace('</style>', 'summary{cursor:pointer;font-size:15px;padding:8px 5px}details{margin:6px 0 14px}#clip-legend p{margin:10px 0}</style>', 1)
  needle = "document.getElementById('buttons').appendChild(button);"
  assert page.count(needle) == 1
  page = page.replace(needle, "document.getElementById(['logged','original','reference_half_denoise'].includes(key)?'buttons':'experiment-buttons').appendChild(button);")
  page = page.replace('One actual comma four microphone recording, with logged-quality, EQ, and archived synthesis comparisons.', 'Compare the logged-quality approximation, native 48 kHz capture, and our current favorite: measured halfway EQ with light denoising. All three use the same recording at matched loudness.')
legend_start = page.index('<div class="note" id="clip-legend">')
legend_end = page.index('</div>', legend_start) + len('</div>')
legend = page[legend_start:legend_end]
entries = {}
for key, clip in clips.items():
  marker = '<p><strong>' + html.escape(clip['title']) + ':</strong>'
  entry_start = legend.index(marker)
  entry_end = legend.index('</p>', entry_start) + len('</p>')
  entries[key] = legend[entry_start:entry_end]
primary = ''.join(entries[key] for key in PRIMARY)
others = ''.join(entry for key, entry in entries.items() if key not in PRIMARY)
legend = '<div class="note" id="clip-legend">' + primary + '<details><summary>Other versions explained</summary>' + others + '</details><p>The measured reference fit includes the iPhone speaker, room, enclosure and microphone. It is not a mic-only calibration.</p></div>'
page = page[:legend_start] + legend + page[legend_end:]
path.write_text(page)
print('Visible:', [clips[key]['title'] for key in PRIMARY])
print('Collapsed experiments:', len(clips)-len(PRIMARY))
