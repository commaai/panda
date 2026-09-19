# Four-way logging comparison

The report now leads with the four requested labeled versions of the same 45-second iPhone capture:

1. Logged equivalent: 16 kHz mono AAC at 32 kbps, no EQ or denoising. This approximates the current format and does not simulate old firmware clicks.
2. Native 48 kHz capture, no EQ or denoising, AAC at the current 32 kbps setting.
3. Preferred halfway reference EQ + light denoise, 48 kHz PCM.
4. The same processed version encoded as 48 kHz AAC at 64 kbps (the former Candidate C).

Compare 1/2 to isolate sample rate at the same nominal codec bitrate, and 3/4 to isolate proposed compression. Playback loudness is matched to approximately −25.70 LUFS. The new 48 kHz / 32 kbps clip measures −2.85 dBFS true peak, and its codec delay compensation aligns within one sample. Earlier clips remain unchanged. All four browser buttons play for 45 seconds and preserve playback position when switching.

The four explanations appear immediately under Restart. Prior blind comparisons and other experiments remain collapsed. Duplicate embedded audio is referenced internally to avoid inflating the report with identical PCM copies. All audio remains embedded. These are offline comparisons, not deployed production changes.

[Listening report](reports/clarity-comparison.html) · [Measurements](metrics/four-way-comparison.json) · [Verification](metrics/four-way-verification.json)

## Added processed 32 kbps comparison

Button 5 adds the same halfway EQ + light denoise at 48 kHz AAC / 32 kbps, reusing the earlier Candidate G exactly. It measures −25.69 LUFS and −3.94 dBFS true peak. Compare 3/5 for compression or 4/5 for bitrate. Its browser playback and 45-second duration were verified; embedded PCM is aliased to avoid increasing report size.

## Added processed 48 kbps comparison

Button 6 adds halfway EQ + light denoise at 48 kHz AAC / 48 kbps, reusing Candidate D exactly. Output measures −25.70 LUFS and −4.65 dBFS true peak; playback and 45-second duration were verified in the browser. The listener preferred 3/4 and, on repeat listening, described 5 as a bit worse. Preference for 6 is pending.

## 32 kHz AAC at 32 kbps

Added button 7 using the same halfway reference EQ + light denoise PCM source. Downsampled to 32 kHz, encoded as mono AAC at 32 kbps, then decoded to 48 kHz WAV for synchronized browser playback. Compare 5 versus 7 for the sample-rate change at the same bitrate; compare 3 versus 7 for overall encoding loss. No new physical recording or production configuration change.

The 45-second encoded M4A is 188,441 bytes (0.251 MB/min including this container), with measured stream bitrate 32,332 bit/s. Playback is matched to -25.70 LUFS, has -3.42 dBFS true peak and zero measured alignment lag. No listening preference established yet. Metrics and reproduction script accompany the embedded report.

## Listening decision: keep 48 kHz

The listener reported hardly any difference between processed 32 kHz AAC/32 kbps (7) and processed 48 kHz AAC/32 kbps (5), with a possible slight preference for 48 kHz; both were acceptable but below the preferred quality. Keep 48 kHz. Removed button 7, its embedded audio, and its legend from the active report; the experiment files and reproduction script remain archived.

Recommend mono AAC at 64 kbps for the next integration trial based on the listener preferring buttons 3/4 (processed PCM/AAC64) and finding AAC32 worse. Nominal audio payload rises from 240 to 480 kB/min, excluding container overhead. This is a listening-based recommendation for this recording, not a production configuration change.
