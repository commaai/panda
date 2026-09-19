"""Blind codec audition; answer key stays outside the shareable report."""
import base64
import concurrent.futures
import hashlib
import json
import random
import subprocess
import wave
from pathlib import Path

import numpy as np

from make_clarity_comparison import loudness, wav_bytes
from add_blind_reference import add_reference

RESULTS = Path('/home/batman/tmp/c4-mic-results')
PAGE_ROOT = RESULTS / 'clarity-comparison-01'
ROOT = RESULTS / 'blind-compression-01'
SOURCE = PAGE_ROOT / 'reference_half_denoise.wav'
RATE = 48000
SAMPLES = RATE * 45
FFMPEG = '/usr/bin/ffmpeg'
ROOT.mkdir(exist_ok=True)
PRIVATE = ROOT / 'private'
PRIVATE.mkdir(exist_ok=True)
SPECS = [
  {'id':'lossless','codec':'flac','options':['-c:a','flac','-compression_level','8'],'extension':'flac'},
  *[{'id':f'aac_{rate}','codec':'aac','bitrate':rate*1000,'options':['-c:a','aac','-b:a',str(rate*1000)],'extension':'m4a'} for rate in (32,48,64,96)],
  *[{'id':f'opus_{rate}','codec':'libopus','bitrate':rate*1000,'options':['-c:a','libopus','-b:a',str(rate*1000),'-vbr','on','-application','audio'],'extension':'opus'} for rate in (32,48)],
]
assignment_file = PRIVATE / 'assignment.json'
if assignment_file.exists():
  assignment = json.loads(assignment_file.read_text())
else:
  identifiers = [spec['id'] for spec in SPECS]
  random.SystemRandom().shuffle(identifiers)
  assignment = dict(zip('ABCDEFG', identifiers, strict=True))
  assignment_file.write_text(json.dumps(assignment,indent=2)+'\n')
with wave.open(str(SOURCE)) as audio:
  assert audio.getframerate()==RATE and audio.getnchannels()==1 and audio.getnframes()==SAMPLES
  source = np.frombuffer(audio.readframes(SAMPLES),dtype='<i2').astype(float)/32768
TARGET = loudness(source)['input_i']


def encode(spec):
  encoded = PRIVATE / (spec['id']+'.'+spec['extension'])
  subprocess.run([FFMPEG,'-v','error','-y','-i',str(SOURCE),'-map_metadata','-1','-ar',str(RATE),'-ac','1',*spec['options'],str(encoded)],check=True)
  raw = subprocess.check_output([FFMPEG,'-v','error','-i',str(encoded),'-f','f64le','-ar',str(RATE),'-ac','1','-'])
  decoded = np.frombuffer(raw,dtype='<f8')
  assert len(decoded)>=SAMPLES
  decoded = decoded[:SAMPLES]
  # Verify codec/container delay compensation on the same non-silent music window.
  start, length = 5*RATE, 4*RATE
  size = 1 << (2*length-1).bit_length()
  correlation = np.fft.irfft(np.fft.rfft(decoded[start:start+length],size)*np.conj(np.fft.rfft(source[start:start+length],size)),size)
  peak = int(np.argmax(correlation))
  lag = peak if peak<size//2 else peak-size
  assert abs(lag)<=1, f'Unexpected codec delay: {lag}'
  gain_db = TARGET-loudness(decoded)['input_i']
  adjusted = decoded*10**(gain_db/20)
  assert abs(adjusted).max()<.95
  if spec['codec']=='flac':
    assert np.array_equal(decoded,source)
    payload=SOURCE.read_bytes()
  else:
    payload=wav_bytes(adjusted)
  (PRIVATE/(spec['id']+'-playback.wav')).write_bytes(payload)
  verified=np.frombuffer(payload[44:],dtype='<i2').astype(float)/32768
  measurements=loudness(verified)
  assert abs(measurements['input_i']-TARGET)<.1 and measurements['input_tp']<-2
  packets=json.loads(subprocess.check_output(['/usr/bin/ffprobe','-v','error','-select_streams','a:0','-show_entries','packet=size','-of','json',str(encoded)]))['packets']
  codec_bytes=sum(int(packet['size']) for packet in packets)
  return spec['id'],payload,{'codec':spec['codec'],'target_bitrate':spec.get('bitrate'),'sample_rate':RATE,'duration_seconds':45,'container_bytes':encoded.stat().st_size,'packet_bytes':codec_bytes,'actual_packet_kbit_s':codec_bytes*8/45/1000,'container_MB_per_minute':encoded.stat().st_size/45*60/1e6,'playback_gain_db':gain_db,'integrated_lufs':measurements['input_i'],'true_peak_dbfs':measurements['input_tp'],'alignment_lag_samples':lag,'playback_sha256':hashlib.sha256(payload).hexdigest()}


with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool:
  encoded = {identifier:(payload,metrics) for identifier,payload,metrics in pool.map(encode,SPECS)}
clips={}
key={}
for label,identifier in assignment.items():
  payload,metrics=encoded[identifier]
  (ROOT/('Candidate-'+label+'.wav')).write_bytes(payload)
  clips['blind_'+label.lower()]={'title':'Candidate '+label,'uri':'data:audio/wav;base64,'+base64.b64encode(payload).decode()}
  key[label]={'identifier':identifier,**metrics}
(PRIVATE/'answer-key.json').write_text(json.dumps({'source_sha256':hashlib.sha256(SOURCE.read_bytes()).hexdigest(),'source':'Reference fit halfway plus light denoise','candidates':key,'scope':'Codec quality/size only. All candidates encode the same 48 kHz denoised EQ listening source. Decoder delay compensation verified. Stored packet sizes exclude container overhead; playback WAV sizes do not represent compressed logging sizes. No production logging integration or CPU validation.'},indent=2)+'\n')
summary={'candidate_count':len(clips),'labels':[clip['title'] for clip in clips.values()],'duration_seconds':45,'sample_rate':RATE,'loudness_target_lufs':TARGET,'max_loudness_spread_lu':max(row['integrated_lufs'] for row in key.values())-min(row['integrated_lufs'] for row in key.values()),'all_alignment_lags_within_one_sample':all(abs(row['alignment_lag_samples'])<=1 for row in key.values()),'all_true_peaks_below_minus_2_dbfs':all(row['true_peak_dbfs']<-2 for row in key.values()),'answer_key_in_report':False}
(ROOT/'verification.json').write_text(json.dumps(summary,indent=2)+'\n')
# Pin the pre-audition report so reruns preserve labels and do not nest bootstraps.
base_report = ROOT / 'base-report.html'
if not base_report.exists():
  base_report.write_text((PAGE_ROOT / 'index.html').read_text())
page = base_report.read_text()
start=page.index('const clips=')+len('const clips=')
end=page.index(';const player=',start)
existing=json.loads(page[start:end])
existing={name:clip for name,clip in existing.items() if not name.startswith('blind_')}
existing.update(clips)
page=page[:start]+json.dumps(existing)+page[end:]
if 'id="blind-comparison"' not in page:
  section='<section id="blind-comparison"><h2>Blind compression comparison</h2><p>Every candidate starts with the same halfway reference EQ + light denoise recording. Labels are randomized and stay fixed across reloads; formats and sizes are hidden. Click to play or switch at the same position. Tell us which letters sound indistinguishable, best, or noticeably worse.</p><div id="blind-buttons"></div><p>Same 45 seconds, matched loudness. The shared player below shows the selected candidate. No automatic playback.</p></section>'
  page=page.replace('<div id="buttons"></div>',section+'<h2>Earlier comparison</h2><div id="buttons"></div>',1)
  previous="document.getElementById(['logged','original','reference_half_denoise'].includes(key)?'buttons':'experiment-buttons').appendChild(button);"
  replacement="document.getElementById(key.startsWith('blind_')?'blind-buttons':(['logged','original','reference_half_denoise'].includes(key)?'buttons':'experiment-buttons')).appendChild(button);"
  assert page.count(previous)==1
  page=page.replace(previous,replacement)
  # Display the player immediately after blind buttons, before the older explanations.
  player_start=page.index('<h2 id="playing">')
  player_end=page.index('<div class="note" id="clip-legend">',player_start)
  player_html=page[player_start:player_end]
  page=page[:player_start]+page[player_end:]
  page=page.replace('</section>',player_html+'</section>',1)
# Put a small bootstrap ahead of the large embedded payload: buttons render while audio loads.
script_start=page.index('<script>const clips=')
script_end=page.index('</script>',script_start)+len('</script>')
full_script=page[script_start:script_end]
data_end=full_script.index(';const player=')
clip_data=full_script[len('<script>const clips='):data_end]
ui=full_script[data_end+1:-len('</script>')]
small_clips={name:{'title':clip['title']} for name,clip in existing.items()}
# Original initialization must wait for the data; clicking early displays a loading message.
ui=ui.replace("player.src=clips[active].uri;", "document.getElementById('error').textContent='Loading embedded audio…';",1)
ui=ui.replace("function choose(key,restart=false){", "function choose(key,restart=false){if(!clips[key].uri){document.getElementById('error').textContent='Audio is still loading; try again shortly.';return;}")
bootstrap='<script>const clips='+json.dumps(small_clips)+';'+ui+'</script>'
loader='<script>const embeddedAudio='+clip_data+';for(const key of Object.keys(embeddedAudio)){clips[key].uri=embeddedAudio[key].uri;}player.src=clips[active].uri;document.getElementById("error").textContent="Ready — choose a version.";</script>'
page=page[:script_start]+loader+page[script_end:]
legend_marker='<div class="note" id="clip-legend">'
page=page.replace(legend_marker,bootstrap+legend_marker,1)
assert len(page.encode())<100*1024*1024
page = add_reference(page)
(PAGE_ROOT/'index.html').write_text(page)
print(json.dumps({**summary,'embedded_report_megabytes':round(len(page.encode())/1e6,2)},indent=2))
