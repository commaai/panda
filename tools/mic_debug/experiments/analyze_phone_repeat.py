import json
import wave
from pathlib import Path

import matplotlib
import matplotlib.pyplot as plt
import numpy as np

matplotlib.use('Agg')
ROOT=Path('/home/batman/tmp/c4-mic-results/phone-repeat-01')
with wave.open(str(ROOT/'captured.wav')) as recording:
  rate=recording.getframerate()
  samples=np.frombuffer(recording.readframes(recording.getnframes()),dtype='<i2').astype(float)/32768
size=4096
hop=480
window=np.hanning(size)
frames=np.lib.stride_tricks.sliding_window_view(samples,size)[::hop]
power=abs(np.fft.rfft(frames*window,axis=1))**2/(rate*np.sum(window**2))
power[:,1:-1]*=2
frequencies=np.fft.rfftfreq(size,1/rate)
times=(np.arange(len(frames))*hop+size/2)/rate
energy=power[:,abs(frequencies-1000)<30].sum(axis=1)
width=round(1.8*rate/hop)
smoothed=np.convolve(energy,np.ones(width),mode='valid')
center=times[np.argmax(smoothed)]+(width-1)*hop/rate/2
start_offset=float(center-3)
results=[]
for index,tone in enumerate([1000,4000,8000,12000,14000,15000,16000,17000,18000,19000,20000]):
  start=start_offset+2+index*3
  segment=samples[round((start+0.4)*rate):round((start+1.6)*rate)]
  quiet=samples[round((start-0.7)*rate):round((start-0.2)*rate)]
  values=[]
  peak=0
  for signal in [segment,quiet]:
    window=np.hanning(len(signal))
    frequency=np.fft.rfftfreq(len(signal),1/rate)
    spectrum=abs(np.fft.rfft(signal*window))**2
    selected=abs(frequency-tone)<25
    rms=np.sqrt(2*np.sum(spectrum[selected])/(len(signal)*np.sum(window**2)))
    values.append(float(20*np.log10(max(rms,1e-15))))
    if len(signal)==len(segment):
      peak=float(frequency[np.flatnonzero(selected)[np.argmax(spectrum[selected])]])
  results.append({'frequency_hz':tone,'peak_hz':peak,'tone_window_band_dbfs':values[0],
                  'preceding_quiet_band_dbfs':values[1],'band_rise_db':values[0]-values[1]})
assert results[0]['band_rise_db'] > 15, 'No verified 1 kHz phone tone; do not interpret this take as a response test'
raw=np.load(ROOT/'captured.npz')
metadata={'scope':'Actual iPhone speaker-to-c4 microphone capture; source file playback triggered remotely after device capture starts. No synthetic injection in firmware.',
          'source_start_seconds_in_capture':start_offset,'tones':results,'overflows':int(np.sum(raw['callbacks'][:,5]))}
(ROOT/'analysis.json').write_text(json.dumps(metadata,indent=2)+'\n')
figure,axes=plt.subplots(2,1,figsize=(12,9),constrained_layout=True)
heatmap=axes[0].pcolormesh(times,frequencies/1000,10*np.log10(np.maximum(power.T,1e-20)),shading='auto',vmin=-145,vmax=-65)
figure.colorbar(heatmap,ax=axes[0],label='Power spectral density (dBFS/Hz)')
axes[0].set(xlabel='Capture time (s)',ylabel='Frequency (kHz)',title='Actual iPhone → comma four microphone tone recording',ylim=(0,24))
axes[1].plot(np.array([item['frequency_hz'] for item in results])/1000,[item['tone_window_band_dbfs'] for item in results],marker='o',label='Tone playing')
axes[1].plot(np.array([item['frequency_hz'] for item in results])/1000,[item['preceding_quiet_band_dbfs'] for item in results],marker='o',label='Preceding quiet interval')
axes[1].set(xlabel='Frequency (kHz)',ylabel='RMS in ±25 Hz band (dBFS)',title='Phone speaker, placement and microphone combined; uncalibrated response')
axes[1].legend()
figure.savefig(ROOT/'phone-response.png',dpi=150)
print(json.dumps(metadata))
