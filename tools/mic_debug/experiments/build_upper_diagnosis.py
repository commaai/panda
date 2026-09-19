import base64
import json
import wave
from pathlib import Path

import matplotlib
import matplotlib.pyplot as plt
import numpy as np

matplotlib.use('Agg')
ROOT=Path('/home/batman/tmp/c4-mic-results')
OUTPUT=ROOT/'upper-frequency-diagnosis-01'
OUTPUT.mkdir(exist_ok=True)
direct=json.loads((ROOT/'digital-path-01/analysis.json').read_text())
exact=json.loads((ROOT/'digital-path-01/bitexact.json').read_text())
resampler=json.loads((ROOT/'resampler-path-02/analysis.json').read_text())
acoustic=json.loads((ROOT/'acoustic-upper-01/analysis.json').read_text())
muted=json.loads((ROOT/'acoustic-muted-01/analysis.json').read_text())
phone=json.loads((ROOT/'phone-auto-02/analysis.json').read_text())
comparison=[]
for active,control in zip(acoustic['tones'],muted['tones'],strict=True):
  comparison.append({'frequency_hz':active['frequency_hz'], 'speaker_enabled_band_dbfs':active['tone_window_band_dbfs'],
                     'speaker_disabled_band_dbfs':control['tone_window_band_dbfs'],
                     'enabled_minus_disabled_db':active['tone_window_band_dbfs']-control['tone_window_band_dbfs'],
                     'enabled_above_preceding_noise_db':active['band_rise_db'], 'disabled_above_preceding_noise_db':control['band_rise_db']})
figure,axes=plt.subplots(2,1,figsize=(12,10),constrained_layout=True)
axes[0].plot(np.array([item['frequency_hz'] for item in direct['tones']])/1000,[item['gain_db'] for item in direct['tones']],label='Measured: I2S → Qualcomm → native PCM')
frequencies=np.array([item['frequency_hz'] for item in resampler['tones']])
gains=np.array([item['gain_db'] for item in resampler['tones']])
axes[0].plot(frequencies/1000,gains,label='Measured: cubic resampler plus downstream path')
mic_clock=240e6/91
modeled=80*np.log10(abs(np.sinc(frequencies*55/mic_clock)/np.sinc(frequencies/mic_clock)))
axes[0].plot(frequencies/1000,gains+modeled,label='Calculated sinc4 loss + measured resampler loss (not an acoustic measurement)')
axes[0].set(xlabel='Frequency (kHz)',ylabel='Gain (dB)',title='Digital stages: no abrupt 16 kHz cutoff',ylim=(-22,2))
axes[0].legend()
frequencies=np.array([item['frequency_hz'] for item in comparison])
axes[1].plot(frequencies/1000,[item['speaker_enabled_band_dbfs'] for item in comparison],label='Speaker enabled: real microphone recording',marker='o')
axes[1].plot(frequencies/1000,[item['speaker_disabled_band_dbfs'] for item in comparison],label='Speaker disabled: same DAC tone sequence',marker='o')
axes[1].plot(frequencies/1000,[item['preceding_quiet_band_dbfs'] for item in acoustic['tones']],label='Preceding room-noise band, speaker-enabled run')
axes[1].set(xlabel='Frequency (kHz)',ylabel='RMS in ±25 Hz band (dBFS)',title='Uncalibrated built-in speaker control; not microphone-only response')
axes[1].legend()
figure.savefig(OUTPUT/'stage-isolation.png',dpi=150)
plt.close(figure)
metadata={'phone_sweep':phone,'direct_bitexact':exact,'resampler':resampler,'speaker_control_comparison':comparison,
          'scope':'Digital synthetic-injection tests and separate real acoustic/muted-control recordings. No production firmware change retained.'}
(OUTPUT/'metrics.json').write_text(json.dumps(metadata,indent=2)+'\n')
page='''<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Comma four upper-frequency diagnosis</title><style>
body{font:17px/1.5 system-ui;max-width:1200px;margin:25px auto;padding:0 20px}img,audio{width:100%}td,th{padding:6px 12px;text-align:right}
</style><h1>Where are the upper frequencies lost?</h1>
<p>The MCU-output → Qualcomm → ALSA path transported 528,000 consecutive samples exactly, including tones through 22 kHz.
A separate injection before the cubic resampler measured gradual attenuation: approximately 1.8 dB near 16 kHz and 4 dB near 20 kHz.
Those tests deliberately use synthetic signals to isolate electronics; they are not acoustic recordings or simulated versions of the music distortion.</p>
<p>The real microphone was then recorded while the built-in speaker played spaced tones, followed by the same DAC sequence with its amplifier disabled.
The comparison below tests whether upper-band pickup depends on speaker operation. It cannot separate airborne sound, mechanical coupling,
or interference originating in the active amplifier. Neither run is a calibrated microphone response measurement.</p>
<p>The original music recording still has very little recoverable content above about 16 kHz. These tests narrow the cause;
they do not establish a microphone-only limit or prove that the iPhone speaker caused it. The automated external-phone sweep below detects 16 and 17 kHz (33 and 28 dB above preceding noise), but 18–20 kHz remain at the noise floor. This still combines source output, geometry, enclosure and microphone response. An independent wideband reference microphone is needed to separate them.</p>
'''
for path in [OUTPUT/'stage-isolation.png',ROOT/'acoustic-upper-01/spectrogram.png',ROOT/'acoustic-muted-01/spectrogram.png',ROOT/'phone-auto-02/phone-response.png']:
  captions = {'stage-isolation.png': 'Digital isolation and built-in c4 speaker control', 'phone-response.png': 'External iPhone speaker: real mic recording', 'spectrogram.png': 'Built-in c4 speaker: ' + ('amplifier disabled' if 'muted' in str(path) else 'amplifier enabled')}
  page += '<h2>' + captions[path.name] + '</h2>'
  page+='<img alt="'+path.stem+'" src="data:image/png;base64,'+base64.b64encode(path.read_bytes()).decode()+'">'
page+='<table><tr><th>Frequency</th><th>Speaker on</th><th>Speaker off</th><th>Difference</th></tr>'
for item in comparison:
  page+=f'<tr><td>{item["frequency_hz"]/1000:g} kHz</td><td>{item["speaker_enabled_band_dbfs"]:.1f} dBFS</td><td>{item["speaker_disabled_band_dbfs"]:.1f} dBFS</td><td>{item["enabled_minus_disabled_db"]:.1f} dB</td></tr>'
page+='</table><h2>Actual phone sweep recording</h2><p>45-second physical microphone recording, native 48 kHz PCM, original level. Phone volume approximately 60%; zero capture overruns. The first Web Audio attempt emitted no detectable tones and is excluded.</p>'
page+='<audio controls preload="none" src="data:audio/wav;base64,'+base64.b64encode((ROOT/'phone-auto-02/captured.wav').read_bytes()).decode()+'"></audio>'
page+='<p>All plots are embedded. The temporary diagnostic firmware was restored to the existing mic pop-fix firmware after each test.</p>'
(OUTPUT/'index.html').write_text(page)
print(json.dumps(comparison))
