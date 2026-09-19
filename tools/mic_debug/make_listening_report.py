#!/usr/bin/env python3
"""Build a local, self-contained listening index from saved microphone artifacts."""
import argparse
import html
import json
from pathlib import Path


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('results', type=Path)
  args = parser.parse_args()
  manifest = json.loads((args.results / 'comparisons/manifest.json').read_text())
  cards = []
  for entry in manifest:
    players = []
    for side in ('before', 'after'):
      path = Path(entry[side])
      if path.name != entry[side] or not (args.results / 'comparisons' / path).is_file():
        raise ValueError(f'Invalid comparison file: {path}')
      players.append(f'<div><b>{side.title()}</b><audio controls preload="none" src="comparisons/{html.escape(path.name)}"></audio></div>')
    cards.append(f'<section><h2>{html.escape(entry["title"])}</h2><div class="pair">{"".join(players)}</div>' +
                 f'<p>{html.escape(entry.get("note", "Separate actual firmware captures; shared playback gain."))}</p></section>')
  measurements = ''
  summary_path = args.results / 'final_summary.json'
  if summary_path.exists():
    summary = json.loads(summary_path.read_text())
    rows = ''.join('<tr>' + ''.join('<td>' + html.escape(str(value)) + '</td>' for value in row) + '</tr>' for row in summary['measurements'])
    measurements = ('<section><h2>Measured results</h2><p>' + html.escape(summary['firmware']) + '</p>' +
                    '<table><thead><tr><th>Measurement</th><th>Result</th><th>Scope</th></tr></thead><tbody>' + rows + '</tbody></table></section>')
  figures = []
  for filename, caption in [
    ('steady_cubic_pair.png', 'Final cubic candidate versus the same native-rate baseline.'),
    ('micd_cubic_codec_comparison.png', 'Final cubic candidate through micd and offline AAC.'),
    ('steady_pair.png', 'Original native-rate pair: the baseline repeats 512 samples. Vertical markers indicate the repeated section boundaries.'),
    ('micd_codec_comparison.png', 'Earlier quiet 16 kHz pair, with offline AAC encoding. The tone phase exposes the discontinuity.'),
    ('resampler_quality_comparison.png', 'Synthetic actual-C measurements, independent of the room. This is not acoustic microphone SNR.'),
  ]:
    if (args.results / filename).is_file():
      figures.append(f'<figure><a href="{filename}"><img src="{filename}" alt="{html.escape(caption)}"></a><figcaption>{caption}</figcaption></figure>')
  document = '''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Comma four microphone investigation</title>
<style>
body { font: 17px/1.5 system-ui, sans-serif; max-width: 1050px; margin: 35px auto; padding: 0 20px; background: #f7f8fa; color: #18202a; }
h1 { line-height: 1.15; } h2 { font-size: 21px; margin-top: 0; }
section, figure { background: white; border: 1px solid #d4d9df; border-radius: 12px; padding: 22px; margin: 22px 0; }
.pair { display: grid; grid-template-columns: repeat(auto-fit,minmax(250px,1fr)); gap: 24px; }
table { width: 100%; border-collapse: collapse; font-size: 15px; }
th, td { text-align: left; padding: 9px; border-bottom: 1px solid #d4d9df; }
audio { display: block; width: 100%; margin-top: 10px; } img { width: 100%; } figcaption, section p { font-size: 14px; color: #46515d; }
.flow { padding: 16px; background: #e9edf2; border-radius: 8px; } a { color: #164fad; }
</style><h1>Comma four microphone investigation</h1>
<p><b>The desk tests found two firmware defects:</b> stale output memory at startup,
and repeated audio blocks caused by independent microphone and output clocks.
Sustained clipping was not observed. The original driving complaint still needs
an affected recording or a confirmed motion reproduction.</p>
<p class="flow">Sound → PDM microphone → STM32 DFSDM / DMA → I2S → Qualcomm DSP / ALSA → PortAudio → micd (16 kHz) → rawAudioData → rlog / AAC video audio</p>
<p>Actual firmware comparisons use separate recordings with shared gain per pair. Room sound can change.
The known steady-tone baseline pop is about 2.1 seconds into Before.
Audio plays on this PC when you press a control; the device has its own playback panel.</p>
'''+measurements+''.join(cards)+''.join(figures)+'''
<p><a href="RESULTS.md">Full measurements, limitations, and capture history</a> · <a href="RESTORE.md">Device state and restore instructions</a></p>
<script>
document.querySelectorAll('audio').forEach(audio => audio.addEventListener('play', () => {
  document.querySelectorAll('audio').forEach(other => {
    if (other !== audio) {
      other.pause();
    }
  });
}));
</script>
</html>'''
  (args.results / 'index.html').write_text(document)


if __name__ == '__main__':
  main()
