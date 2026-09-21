"""Show the four requested logging/processing stages using one real recording."""
import base64
import hashlib
import html
import json
import re
import shutil
import subprocess
import wave
from pathlib import Path

import numpy as np

from make_clarity_comparison import loudness, wav_bytes

RESULTS=Path('/home/batman/tmp/c4-mic-results')
PAGE_ROOT=RESULTS/'clarity-comparison-01'
ROOT=RESULTS/'four-way-01'
ROOT.mkdir(exist_ok=True)
SOURCE=RESULTS/'phone-music-camera-01/captured.wav'
RATE=48000
FFMPEG='/usr/bin/ffmpeg'
TARGET=-25.70
encoded=ROOT/'native48-aac32.m4a'
subprocess.run([FFMPEG,'-v','error','-y','-i',str(SOURCE),'-ss','3.48','-t','45','-ar',str(RATE),'-ac','1','-map_metadata','-1','-c:a','aac','-b:a','32000',str(encoded)],check=True)
decoded=np.frombuffer(subprocess.check_output([FFMPEG,'-v','error','-i',str(encoded),'-f','f64le','-ar',str(RATE),'-ac','1','-']),dtype='<f8')[:45*RATE]
assert len(decoded)==45*RATE
measurements=loudness(decoded)
signal=decoded*10**((TARGET-measurements['input_i'])/20)
assert abs(signal).max()<.95
payload=wav_bytes(signal)
(ROOT/'native48-aac32.wav').write_bytes(payload)
verified=loudness(np.frombuffer(payload[44:],dtype='<i2').astype(float)/32768)
assert abs(verified['input_i']-TARGET)<.1 and verified['input_tp']<-2.5
PRIMARY={
  'logged':'1. Logged equivalent — 16 kHz AAC / 32 kbps',
  'native48_aac32':'2. Native 48 kHz — AAC / 32 kbps',
  'reference_half_denoise':'3. Halfway EQ + denoise — 48 kHz PCM',
  'proposed48_aac64':'4. Proposed EQ + denoise — 48 kHz AAC / 64 kbps',
}
DESCRIPTIONS={
  'logged':'Current logged-format approximation: 16 kHz mono AAC at 32 kbps, with no EQ or denoising. Uses this same recording; old firmware clicks are not recreated.',
  'native48_aac32':'Native 48 kHz capture encoded with the current qcamera AAC bitrate of 32 kbps. No EQ or denoising. Compare 1 versus 2 to hear the sample-rate change at the same bitrate.',
  'reference_half_denoise':'The exact halfway reference EQ + light denoise PCM version you liked. This is the uncompressed processed listening target.',
  'proposed48_aac64':'That same processed target encoded at 48 kHz with AAC at 64 kbps, then decoded for playback. Compare 3 versus 4 to isolate compression. Same audio as the earlier Candidate C.',
}
paths={
  'logged':PAGE_ROOT/'logged-equivalent.wav',
  'native48_aac32':ROOT/'native48-aac32.wav',
  'reference_half_denoise':PAGE_ROOT/'reference_half_denoise.wav',
  'proposed48_aac64':RESULTS/'blind-compression-01/private/aac_64-playback.wav',
}
rows=[]
for key,path in paths.items():
  with wave.open(str(path)) as audio:
    assert audio.getnframes()==45*audio.getframerate()
  if path.parent!=ROOT:
    shutil.copy2(path,ROOT/(key+'.wav'))
  rows.append({'key':key,'title':PRIMARY[key],'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'description':DESCRIPTIONS[key]})
metrics={'duration_seconds':45,'loudness_target_lufs':TARGET,'new_native48_aac32':{'integrated_lufs':verified['input_i'],'true_peak_dbfs':verified['input_tp'],'encoded_container_bytes':encoded.stat().st_size},'variants':rows,'scope':'Offline format comparisons on the same captured music. Not recordings of production logging changes. Existing favorite and proposed 64 kbps playback are reused exactly.'}
(ROOT/'metrics.json').write_text(json.dumps(metrics,indent=2)+'\n')
page=(PAGE_ROOT/'index.html').read_text()
start=page.index('const embeddedAudio=')+len('const embeddedAudio=')
end=page.index(';for(const key of Object.keys(embeddedAudio))',start)
original=json.loads(page[start:end])
# Resolve aliases if this script is run again.
for key,clip in original.items():
  if 'aliasOf' in clip:
    clip['uri']=original[clip['aliasOf']]['uri']
clips={key:{'title':title,'uri':'data:audio/wav;base64,'+base64.b64encode(paths[key].read_bytes()).decode()} for key,title in PRIMARY.items()}
clips.update({key:clip for key,clip in original.items() if key not in PRIMARY})
# Avoid embedding duplicate PCM for the labeled and anonymous copies.
seen={}
embedded={}
for key,clip in clips.items():
  digest=hashlib.sha256(clip['uri'].encode()).hexdigest()
  if digest in seen:
    embedded[key]={'title':clip['title'],'aliasOf':seen[digest]}
  else:
    seen[digest]=key
    embedded[key]={'title':clip['title'],'uri':clip['uri']}
page=page[:start]+json.dumps(embedded)+page[end:]
loader='for(const key of Object.keys(embeddedAudio)){clips[key].uri=embeddedAudio[key].uri;}'
replacement='for(const key of Object.keys(embeddedAudio)){clips[key].uri=embeddedAudio[key].uri;}for(const key of Object.keys(embeddedAudio)){if(embeddedAudio[key].aliasOf){clips[key].uri=clips[embeddedAudio[key].aliasOf].uri;}}'
if replacement not in page:
  assert loader in page
  page=page.replace(loader,replacement,1)
start=page.index('const clips=')+len('const clips=')
end=page.index(';const player=',start)
page=page[:start]+json.dumps({key:{'title':clip['title']} for key,clip in clips.items()})+page[end:]
page=page.replace("['logged','original','reference_half_denoise'].includes(key)","['logged','native48_aac32','reference_half_denoise','proposed48_aac64'].includes(key)")
page=page.replace("let active='original';","let active='logged';")
page=re.sub(r'<h2 id="playing">.*?</h2>','<h2 id="playing">'+html.escape(PRIMARY['logged'])+'</h2>',page,count=1)
if 'id="four-way-comparison"' not in page:
  player_start=page.index('<h2 id="playing">')
  player_end=page.index('</section>',player_start)
  player_html=page[player_start:player_end]
  page=page[:player_start]+page[player_end:]
  page=page.replace('<h2>Earlier comparison</h2><div id="buttons"></div>','',1)
  marker='<section id="blind-comparison">'
  main='<section id="four-way-comparison"><h2>Four-way logging comparison</h2><p>Same 45 seconds and matched loudness. Switch at the same playback position. Compare 1 → 2 for sample rate, and 3 → 4 for the proposed compression.</p><div id="buttons"></div>'+player_html+'</section>'
  page=page.replace(marker,main+'<details id="earlier-blind"><summary>Earlier blind compression comparison</summary>'+marker,1)
  blind_start=page.index(marker)
  blind_end=page.index('</section>',blind_start)+len('</section>')
  page=page[:blind_end]+'</details>'+page[blind_end:]
  page=page.replace('More experiments (7)','More experiments (8)')
legend_start=page.index('<div class="note" id="clip-legend">')
legend_end=page.index('</div>',legend_start)+len('</div>')
old_legend=page[legend_start:legend_end]
extras=re.search(r'<details>.*?</details>',old_legend,re.S).group(0)
if '<strong>Native 48 kHz PCM:</strong>' not in extras:
  extras=extras.replace('</summary>','</summary><p><strong>Native 48 kHz PCM:</strong> Experimental native capture with playback gain only, no EQ or denoising.</p>',1)
legend='<div class="note" id="clip-legend">'+''.join('<p><strong>'+html.escape(title)+':</strong> '+html.escape(DESCRIPTIONS[key])+'</p>' for key,title in PRIMARY.items())+extras+'<p>Reference EQ includes the phone speaker, room and mic response; this is not a mic-only calibration.</p></div>'
page=page[:legend_start]+page[legend_end:]
restart_marker='<button id="restart">Restart this version</button>'
page=page.replace(restart_marker,restart_marker+legend,1)
page=page.replace('Compare the logged-quality approximation, native 48 kHz capture, and our current favorite: measured halfway EQ with light denoising. All three use the same recording at matched loudness.','Compare current logged quality, native-rate capture at the current AAC bitrate, the preferred processed PCM, and the proposed compressed version.')
page=page.replace('with at least approximately 3 dB true-peak headroom.', 'with at least 2.8 dB measured true-peak headroom.')
page=page.replace('No limiting or compression. Only the explicitly labeled light-denoise version uses noise suppression.','No dynamic-range compression or limiting. EQ + denoise versions use the same noise suppression; AAC is the indicated lossy audio encoding.')
page=page.replace('preference is pending.','the listener preferred this version at a slightly lower playback volume.')
assert len(page.encode())<100*1024*1024
pending=PAGE_ROOT/'index.pending.html'
pending.write_text(page)
pending.replace(PAGE_ROOT/'index.html')
print(json.dumps({'buttons':list(PRIMARY.values()),'native48_aac32_measured':metrics['new_native48_aac32'],'report_MB':round(len(page.encode())/1e6,2)},indent=2))
