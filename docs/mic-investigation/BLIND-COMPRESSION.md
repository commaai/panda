# Blind compression audition

Seven anonymous candidates use the same 45-second halfway-reference-EQ plus light-denoise recording, which the listener preferred at a slightly lower playback volume. Candidate assignments are randomized once and persist across reloads. The answer key is retained only in the local investigation archive while listening is underway.

Each codec output is decoded into the same 48 kHz mono 16-bit PCM playback container, so browser codec handling, filenames and displayed file sizes do not disclose the candidate identity. Playback WAV sizes do not represent proposed log-storage sizes. Integrated loudness is matched within 0.01 LU; decoder delay compensation is checked to within one sample. All candidates retain more than 2 dB measured true-peak headroom. No extra synthesis is used.

The report contains only Candidate A–G labels. Buttons share one player, retain time when switching, and do not autoplay. All seven browser buttons were verified muted, with 45-second duration and no page errors. Controls now initialize before the large embedded audio payload finishes loading. The existing three main comparison buttons and collapsed experiments remain available.

This audition compares encoding quality for one recording. It does not validate production integration, runtime cost, other sounds, or raw-rlog compression savings. Compressed sizes and identities can be revealed after listening.

[Self-contained listening report](reports/clarity-comparison.html) · [Blinded verification](metrics/blind-compression-verification.json)

A labeled “Reference — halfway EQ + light denoise (48 kHz PCM)” button precedes A–G. It reuses the exact preferred processed PCM clip, verified in the browser, and does not disclose the anonymous candidate identities.
