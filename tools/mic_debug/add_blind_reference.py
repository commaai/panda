"""Add a labeled, uncompressed processed reference without disclosing blind labels."""
from pathlib import Path

ROOT = Path('/home/batman/tmp/c4-mic-results/clarity-comparison-01')


def add_reference(page):
  if 'knownReferenceButton' in page:
    return page
  marker = "document.getElementById('restart').onclick=()=>choose(active,true);"
  assert page.count(marker) == 1
  script = "const knownReferenceButton=document.createElement('button');knownReferenceButton.textContent='Reference — halfway EQ + light denoise (48 kHz PCM)';knownReferenceButton.dataset.clip='reference_half_denoise';knownReferenceButton.setAttribute('aria-pressed','false');knownReferenceButton.onclick=()=>choose('reference_half_denoise');document.getElementById('blind-buttons').prepend(knownReferenceButton);"
  page = page.replace(marker, script + marker, 1)
  page = page.replace('Labels are randomized and stay fixed across reloads; formats and sizes are hidden.', 'Use the labeled uncompressed reference as your listening target. Candidate A–G labels are randomized and stay fixed across reloads; their formats and sizes are hidden.')
  return page


if __name__ == '__main__':
  path = ROOT / 'index.html'
  path.write_text(add_reference(path.read_text()))
  print('Added labeled processed PCM reference; anonymous labels unchanged.')
