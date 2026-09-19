# Four-way logging comparison

The report now leads with the four requested labeled versions of the same 45-second iPhone capture:

1. Logged equivalent: 16 kHz mono AAC at 32 kbps, no EQ or denoising. This approximates the current format and does not simulate old firmware clicks.
2. Native 48 kHz capture, no EQ or denoising, AAC at the current 32 kbps setting.
3. Preferred halfway reference EQ + light denoise, 48 kHz PCM.
4. The same processed version encoded as 48 kHz AAC at 64 kbps (the former Candidate C).

Compare 1/2 to isolate sample rate at the same nominal codec bitrate, and 3/4 to isolate proposed compression. Playback loudness is matched to approximately −25.70 LUFS. The new 48 kHz / 32 kbps clip measures −2.85 dBFS true peak, and its codec delay compensation aligns within one sample. Earlier clips remain unchanged. All four browser buttons play for 45 seconds and preserve playback position when switching.

The four explanations appear immediately under Restart. Prior blind comparisons and other experiments remain collapsed. Duplicate embedded audio is referenced internally to avoid inflating the report with identical PCM copies. All audio remains embedded. These are offline comparisons, not deployed production changes.

[Listening report](reports/clarity-comparison.html) · [Measurements](metrics/four-way-comparison.json) · [Verification](metrics/four-way-verification.json)
