# Remotely controlled phone speaker

Serve this directory plus `phone-sweep.wav` on the comma four's local LAN address. The phone opens the page and taps **Enable speaker** once. Playback is then controlled by an atomically replaced `command.json` on the device; HTTP clients cannot issue playback commands. Commands target one random browser client ID. The page has a stop/disconnect button, stops on lost connectivity or backgrounding, and sends readiness/playback events.

The current implementation uses the same unlocked HTMLAudioElement for subsequent playback, with `navigator.audioSession.type = 'playback'` where supported. The initial Web Audio version reported playback without emitting audible phone sound in this session, so its first recording is excluded from frequency-response analysis. Browser acknowledgment is not a substitute for verifying tones in the physical recording.

Keep-awake uses native Screen Wake Lock where available, otherwise an unmuted video containing an exactly silent AAC track. It periodically seeks within the clip to avoid an end-of-playback interval. An earlier muted, video-only loop did not prevent the user's iPhone from timing out. The generated clip's decoded audio was checked to be all zeros. Keep Safari in the foreground; switching apps disconnects intentionally.

Run on the device (all files under `/data`):

```sh
python3 server.py /data/mic-lab/phone-sweep-01 --bind 192.168.63.47 --port 8767
python3 record.py /data/mic-lab/phone-auto-02
```

`record.py` requires exactly one fresh, armed iPhone client; local `start.wav` and `done.wav` TTS cues; the existing capture probe at `/data/mic-lab/phone-capture-01/capture.py`; and the restored 77581827 mic firmware. It announces the recording, opens 48 kHz capture, then commands phone playback after the capture stream reports ready. It records acknowledgments and always stops phone playback on exit. It does not change system volume, mixer settings, firmware, or phone microphone permissions.

The prepared stimulus is 35 seconds at 48 kHz, with 0.1 full-scale peak tones and smooth fades. The user reported approximately 60% physical phone volume. Keep placement and volume fixed for repeat measurements.

The fallback follows the media-playback approach documented by [NoSleep.js](https://github.com/richtr/NoSleep.js); its code/assets are not bundled. Safari's Web Audio mute-policy issue is documented in [WebKit bug 237322](https://bugs.webkit.org/show_bug.cgi?id=237322). Screen Wake Lock generally requires [a secure context](https://developer.mozilla.org/en-US/docs/Web/API/Screen_Wake_Lock_API), so the silent-media fallback matters for this HTTP LAN page.

## Selecting subsequent stimuli

Protocol 2 permits a local command to select a `phone-[a-z0-9-]+.wav` file from the server directory. The HTTP server serves only this filename pattern and its fixed UI assets; recording files are not exposed. Reload and enable once when upgrading from protocol 1. Subsequent source changes reuse the unlocked native audio element without another tap. The recorder sizes the capture from the selected WAV duration plus ten seconds, and requires protocol 2 for custom sources:

```sh
python3 record.py /data/mic-lab/phone-music-01 --source phone-music.wav
```

The music and three-level tone runs use normal physical microphone capture and the existing pop-fix firmware. Always check acoustic content as well as browser acknowledgments. The earlier built-in-speaker sweep is a different source/coupling experiment and must not be presented as an equivalent iPhone measurement.
