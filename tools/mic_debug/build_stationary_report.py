import base64
import json
from pathlib import Path

RESULTS = Path('/home/batman/tmp/c4-mic-results')
TONES = RESULTS / 'headphone-stationary-tones-01'
MUSIC = RESULTS / 'headphone-stationary-music-01'
ROOT = RESULTS / 'headphone-stationary-report'
ROOT.mkdir(exist_ok=True)
tones = json.loads((TONES / 'metrics.json').read_text())
music = json.loads((MUSIC / 'metrics.json').read_text())
previous = json.loads((RESULTS / 'headphone-music-01/metrics.json').read_text())
current = music['captures']['headphone-stationary-music-01']
comparisons = []
for old, new in zip(previous['captures']['headphone-music-01']['bands'], current['bands'], strict=True):
  comparisons.append({'band_hz': new['band_hz'], 'previous_music_dbfs': old['music_dbfs'], 'stationary_music_dbfs': new['music_dbfs'], 'difference_db': new['music_dbfs'] - old['music_dbfs'], 'stationary_music_above_quiet_db': new['rise_db']})
metrics = {'tones': tones, 'music': music, 'comparison_with_previous_moved_run': comparisons, 'conditions': 'User confirmed stationary before paired tone and music recordings; asked to keep earcup fixed throughout. Same stimulus files, firmware and requested PC sink. Captured sink volume recorded separately for both runs; source uncalibrated.'}
(ROOT / 'metrics.json').write_text(json.dumps(metrics, indent=2) + '\n')
page = (MUSIC / 'index.html').read_text().replace('<title>AirPods versus iPhone music capture</title>', '<title>Stationary AirPods: tones and music</title>').replace('<h1>Same music: AirPods versus iPhone</h1>', '<h1>Stationary AirPods: tones and music</h1>')
extra = '<h2>Fresh stationary tone recording</h2><p>Recorded immediately before the music. Tone file peak 0.1; music file peak 0.2. The earcup was to remain in place through both captures. Digital file levels do not establish acoustic sound pressure.</p>'
extra += '<img alt="Actual stationary tone recording" src="data:image/png;base64,' + base64.b64encode((TONES / 'headphone-tones.png').read_bytes()).decode() + '">'
extra += '<audio controls preload="none" src="data:audio/wav;base64,' + base64.b64encode((TONES / 'captured.wav').read_bytes()).decode() + '"></audio><p>Tone recording player uses original capture level, without amplification.</p>'
extra += '<table><tr><th>Tone (kHz)</th><th>Rise above preceding quiet (dB)</th></tr>'
for row in tones['tones']:
  extra += f'<tr><td>{row["frequency_hz"]/1000:g}</td><td>{row["rise_db"]:.1f}</td></tr>'
extra += '</table>'
page += extra
(ROOT / 'index.html').write_text(page)
print(json.dumps({'stationary_tone_rises': [(row['frequency_hz'], round(row['rise_db'], 1)) for row in tones['tones']], 'music_correlation': current['envelope_correlation'], 'music_bands': current['bands'], 'prior_vs_stationary': comparisons}, indent=2))
