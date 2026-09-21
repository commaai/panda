# Hackathon tooling snapshot

These tools depend on the openpilot Python environment, device services and the session paths documented below. They are included here alongside the Panda investigation for reproducibility; they are not part of the Panda firmware build. The experiments directory preserves scripts with local session paths. Use ~/tmp for PC artifacts and /data/mic-lab on device.

# Microphone capture probe

Run from the checkout with its Python environment:

```sh
python tools/mic_debug/probe.py --host comma@192.168.63.47 --output ~/tmp/c4-mic-results/baseline --seconds 20 --replay
python tools/mic_debug/probe.py --host comma@192.168.63.47 --output ~/tmp/c4-mic-results/micd --seconds 20 --workload micd --stimulus tones --replay
python tools/mic_debug/probe.py --host comma@192.168.63.47 --output ~/tmp/c4-mic-results/native48 --seconds 30 --rate 48000 --dtype int16 --stimulus tones --replay
```

The controller copies a temporary capture script to a private directory under `/data/mic-lab/captures` on the device and downloads the results. The remote directory is removed after successful download and analysis; `--keep-remote` retains it. `--remote-root` overrides the parent directory. The default avoids the device's small `/tmp` tmpfs. It does not edit openpilot, parameters, or mixer controls. Use a distinct output prefix for each experiment. The device must have a free capture stream; inspect running processes before using it.

Outputs:

- `.wav`: playable 16-bit recording, using micd's conversion for float input.
- `.npz`: original samples before integer conversion, callback timing/status, and optional playback reference.
- `.json`: requested and actual stream configuration.
- `_analysis.json`: level, near-rail counts, callback overruns, and discontinuity candidates.
- `.png`: waveform envelope, spectrum, largest jump, and callback timing.

`--workload micd` executes the installed `Mic.callback`, including its loudness calculation, with a capture publisher replacing IPC. This isolates that callback's transformations; it does not reproduce loggerd or the real IPC workload. `--workload capture` only copies samples and timing information into preallocated arrays.

At 48 kHz, analysis also searches for exact nonconstant runs repeated after 512 or 1024 samples, matching the Panda microphone DMA buffer sizes. Constant captures are flagged: a successful ALSA capture does not establish that Panda and the microphone are running. `--replay` plays the first 15 seconds of the saved clip (adjust with `--replay-seconds`) on the device after capture, adjusting peak level to at most the requested test volume with amplification capped at 4x.

Optional settings: `--device`, `--rate`, `--channels`, `--dtype`, `--blocksize`, `--latency`. A silent 48 kHz playback stream runs by default: comma four's Panda firmware enables microphone sampling from its playback DMA handler. `--no-playback` disables it for diagnostics. Playback tones use a default peak amplitude of 0.03. A speaker and noisy room are not calibrated acoustic references: use these captures for controlled comparisons, not absolute microphone distortion measurements. Sample jumps can be real sounds, and callback timestamp variation alone is not proof of missing audio.

Reanalyze a saved capture:

```sh
python tools/mic_debug/probe.py --analyze-only --output ~/tmp/c4-mic-results/baseline
```

Quality reports include peak/RMS/DC/crest factor, clipping counts, capture overruns, startup-excluded levels, and exact repeated-block events at 48 kHz. Optional tones add frequency error, apparent SNR, THD and THD+N for the whole speaker-room-microphone path. A tone must stand above room noise before those distortion estimates are useful; `tone_above_room_noise` records that check. Background RMS is room noise plus microphone noise, not microphone self-noise or calibrated SPL.

Optional live status windows (run `status_display.py` on each host with its openpilot environment):

```sh
python tools/mic_debug/status_display.py ~/tmp/c4-mic-results/status.json
python tools/mic_debug/status.py --host comma@192.168.63.47 --title "Recording" --message "Five-minute background test" --duration 300 --notify
```

On the device use `/data/mic-lab/status.json`. All device files, locks, clips, and capture data belong under `/data/mic-lab`; never use device `/tmp`. The display can play the Before/After WAV files listed in an optional `--comparisons DIR` manifest. Each comparison records its shared playback gain and source captures. Offline repair illustrations are labeled explicitly. `status.py --allow-playback` enables the controls between tests; omit it while measuring. Device capture and comparison playback also share an exclusive audio lock. The updater only changes status text and optional desktop notifications. Close the desktop window normally; stop the dedicated status-display process on the device before restarting its regular UI. Recording/replay is controlled separately by `probe.py`.

The drift-correction prototype has a reproducible host check using its actual C handler code with simulated DMA input. It runs 320 seconds per clock ratio from -2000 to +2000 ppm, with input-position jitter, under UndefinedBehaviorSanitizer:

```sh
python tools/mic_debug/check_resampler.py ~/tmp/c4-mic-panda/board/stm32h7/sound.h
```

This simulation checks sample continuity and retained tone frequency; it does not model concurrent DMA writes, interrupt preemption, or CAN load. The device recordings and temporary firmware counters complement it. `--panda-debug` only works with the explicitly instrumented MICRESAMPLEDBG firmware and stores its buffer-resync, occupancy, rate-correction, and interrupt-duration counters.

A controlled clock-discontinuity test can use `--stimulus steady-tone`: a continuous 1 kHz tone between one-second silent margins, with 100 ms fades. Use identical volume/settings before and after. This makes missing/repeated waveform sections easier to locate without relying on changing speech; it is not proof that a driver's reported pop has the same cause.

For a later physical-motion test, `motion_logger.py /data/mic-lab/movement --seconds 75` records acceleration on the device while `sensord` is stopped. It restores the accelerometer registers afterward. Its `monotonic_seconds` column shares the device clock with the audio callback diagnostics. It captures gross motion at approximately 104 Hz, with ±2g range; polling timestamps and bandwidth do not support a calibrated vibration measurement.

Run the real logger path with isolated test parameters and logs:

```sh
python tools/mic_debug/run_logger_probe.py --host comma@192.168.63.47 --output ~/tmp/c4-mic-results/logger-test --seconds 60 --load
```

The optional `--load` starts a bounded CPU-only workload on each available core. The controller downloads the test directory and restores the listening buttons on exit. The remote helper uses private `/data/mic-lab` paths for recordings, logs, and parameters, plus a unique runtime IPC directory under `/dev/shm` that it removes after stopping its processes. It compares the real rlog PCM with the captured callback output byte-for-byte. `--workload micd-ipc` forwards actual messages and uses an explicit `--ipc-warmup` (default 1.2 seconds) to let subscribers settle before capture. This is a test-harness barrier, not normal micd behavior. A startup message-loss observation is retained in RESULTS.md.

Measure the source resampler separately from room and speaker quality:

```sh
python tools/mic_debug/check_resampler_quality.py ~/tmp/c4-mic-panda/board/stm32h7/sound.h --output ~/tmp/resampler-quality.json
```

This compiles the actual microphone handlers with synthetic DMA/sample input and measures 1, 4, and 7.5 kHz tones after settling. It includes rate-control and quantization error; it is not acoustic microphone SNR. The report records the tested source hash. The listening display's Next button cycles through two comparisons per page.

Build a local listening page from the saved results and comparison manifest:

```sh
python tools/mic_debug/make_listening_report.py ~/tmp/c4-mic-results
xdg-open ~/tmp/c4-mic-results/index.html
```

The page uses ordinary browser audio controls and local images; it does not require a server or publish recordings. The final saved investigation also includes `final_summary.json` for the measured-results table.

## Native quality comparison

`quality_report.py native.wav output_directory` builds a self-contained listening report from a 48 kHz mono PCM16 capture. It compares that identical sound at native rate, 16 kHz PCM, AAC at 32/96 kbit/s, and mild experimental FFT denoising. It uses system FFmpeg and the existing NumPy/Matplotlib environment. The players use a shared constant gain; the raw source is preserved. This isolates offline processing, not the microphone's calibrated acoustic limits.

The saved `session_2026_09_18/quality-baseline-01/QUALITY.md` identifies the schematic microphone, its datasheet limits, the decimator response calculation, and remaining acoustic measurements. Its comparison report and raw capture are included in the local evidence archive. Announce each future recording with spoken TTS and a countdown on the device, at full digital volume.
