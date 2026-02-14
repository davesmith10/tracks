# TRACKS — Initial Event Planning Document

## Context

The TRACKS project aims to create a C++ command-line tool that processes audio files (WAV, MP3) using Essentia's streaming mode and emits a stream of structured event messages in real-time (at play speed). The Essentia static library has been successfully built at `essentia/build/src/libessentia.a` with 282 available algorithms. No application code exists yet.

This plan is the **first task**: cataloging all feasible music events that TRACKS can detect, organized by category, with notes on how each would be constructed from Essentia's streaming algorithms.

---

## Event Catalog

### Category 1: Transport Events (Application-Level)
These are not MIR-derived but are fundamental to the event stream.

| Event | Description | Source |
|-------|-------------|--------|
| `track.start` | Playback begins, includes metadata (filename, duration, sample rate, channels) | `MetadataReader`, `Duration` |
| `track.end` | End of file reached | Application logic |
| `track.position` | Periodic time position heartbeat (e.g., every N frames) | Frame counter |

### Category 2: Beat & Rhythm Events
Core rhythmic structure detection.

| Event | Algorithm(s) | Output Type | Notes |
|-------|-------------|-------------|-------|
| `beat` | `BeatTrackerDegara` or `BeatTrackerMultiFeature` | Event (timestamp) | Emitted at each detected beat position |
| `tempo.change` | `RhythmExtractor2013`, `BpmHistogram` | Event (new BPM value) | Emitted when estimated tempo changes significantly |
| `downbeat` | Derived from beat tracker + meter estimation | Event | First beat of a measure (requires beat grouping logic) |

### Category 3: Onset Events
Detection of note/sound attacks and transients.

| Event | Algorithm(s) | Output Type | Notes |
|-------|-------------|-------------|-------|
| `onset` | `SuperFluxExtractor`, `OnsetDetection`, `Onsets` | Event (timestamp, strength) | Individual sound onsets |
| `onset.rate` | `OnsetRate` | Continuous | Onsets per second — useful for detecting busy vs. sparse passages |
| `novelty` | `NoveltyCurve` | Continuous (per-frame) | Onset strength function; high values = likely onset |

### Category 4: Tonal & Harmonic Events
Pitch class, key, and chord detection.

| Event | Algorithm(s) | Output Type | Notes |
|-------|-------------|-------------|-------|
| `key.change` | `Key` (fed by `HPCP`) | Event | Emitted when detected key changes (e.g., C major → A minor) |
| `chord.change` | `ChordsDetection` (fed by `HPCP`) | Event (chord label) | Emitted when chord changes (e.g., "Am" → "G") |
| `chroma` | `HPCP` | Continuous (12-dim vector) | Harmonic pitch class profile per frame |
| `tuning` | `TuningFrequency` | Event | Detected tuning deviation from A440 |
| `dissonance` | `Dissonance` | Continuous (per-frame) | Sensory dissonance level |
| `inharmonicity` | `Inharmonicity` | Continuous (per-frame) | How far partials deviate from harmonic series |

### Category 5: Pitch & Melody Events
Fundamental frequency and melodic contour tracking.

| Event | Algorithm(s) | Output Type | Notes |
|-------|-------------|-------------|-------|
| `pitch` | `PitchYin` or `PitchYinFFT` | Continuous (Hz + confidence) | Frame-by-frame pitch estimate |
| `pitch.change` | Derived from `PitchYin` | Event | Emitted when pitch changes significantly (note transition) |
| `melody` | `PitchMelodia` or `PredominantPitchMelodia` | Continuous (Hz) | Predominant melody line extraction |

### Category 6: Loudness & Energy Events
Dynamics, energy, and level tracking.

| Event | Algorithm(s) | Output Type | Notes |
|-------|-------------|-------------|-------|
| `loudness` | `Loudness` or `LoudnessEBUR128` | Continuous (dB/LUFS) | Frame-by-frame loudness |
| `loudness.peak` | Derived from `Loudness` | Event | Local loudness maximum (accent/hit) |
| `energy` | `Energy`, `InstantPower` | Continuous | Signal energy per frame |
| `dynamic.change` | `DynamicComplexity` or derived | Event | Significant dynamic shift (e.g., quiet→loud transition) |

### Category 7: Silence & Gap Events
Detection of non-sound regions.

| Event | Algorithm(s) | Output Type | Notes |
|-------|-------------|-------------|-------|
| `silence.start` | `SilenceRate`, `StartStopSilence` | Event | Audio drops below silence threshold |
| `silence.end` | `SilenceRate`, `StartStopSilence` | Event | Audio rises above silence threshold |
| `gap` | `GapsDetector` | Event (start, end, duration) | Detected gap/pause in audio |

### Category 8: Spectral Feature Events
Timbral and spectral characteristics that change over time.

| Event | Algorithm(s) | Output Type | Notes |
|-------|-------------|-------------|-------|
| `spectral.centroid` | `SpectralCentroidTime` | Continuous | "Brightness" of sound |
| `spectral.flux` | `SpectralFlux` | Continuous | Rate of spectral change (related to onsets) |
| `spectral.complexity` | `SpectralComplexity` | Continuous | Spectral complexity measure |
| `spectral.contrast` | `SpectralContrast` | Continuous (multi-band) | Energy distribution across frequency bands |
| `spectral.rolloff` | Derived from `Spectrum` | Continuous | Frequency below which N% of energy lies |
| `mfcc` | `MFCC` | Continuous (13-dim vector) | Mel-frequency cepstral coefficients (timbre fingerprint) |
| `timbre.change` | Derived from MFCC distance | Event | Significant timbral shift detected |

### Category 9: Frequency Band Energy
Energy distribution across perceptual frequency bands.

| Event | Algorithm(s) | Output Type | Notes |
|-------|-------------|-------------|-------|
| `bands.mel` | `MelBands` | Continuous (multi-band) | Mel-scale energy distribution |
| `bands.bark` | `BarkBands` | Continuous (multi-band) | Bark-scale energy distribution |
| `bands.erb` | `ERBBands` | Continuous (multi-band) | ERB-scale energy distribution |
| `hfc` | `HighFrequencyContent` | Continuous | High-frequency energy (correlates with percussive content) |

### Category 10: Structural / Segmentation Events
Large-scale structural boundaries in the audio.

| Event | Algorithm(s) | Output Type | Notes |
|-------|-------------|-------------|-------|
| `segment.boundary` | `SBic` | Event (timestamp) | Structural change detected (e.g., verse→chorus) |
| `fade.in` | `FadeDetection` | Event (start, end) | Fade-in detected |
| `fade.out` | `FadeDetection` | Event (start, end) | Fade-out detected |

### Category 11: Audio Quality / Artifact Events
Detection of problems or anomalies in the audio signal.

| Event | Algorithm(s) | Output Type | Notes |
|-------|-------------|-------------|-------|
| `click` | `ClickDetector` | Event (timestamp) | Impulsive noise (click/pop) detected |
| `discontinuity` | `DiscontinuityDetector` | Event (timestamp) | Signal discontinuity detected |
| `noise.burst` | `NoiseBurstDetector` | Event (timestamp) | Noise burst detected |
| `saturation` | `SaturationDetector` | Event (start, end) | Clipping/saturation region detected |
| `hum` | `HumDetector` | Event | Persistent low-frequency tonal noise detected |

### Category 12: Envelope & Transient Events
Signal envelope characteristics.

| Event | Algorithm(s) | Output Type | Notes |
|-------|-------------|-------------|-------|
| `envelope` | `Envelope` | Continuous | Signal envelope per frame |
| `attack` | `LogAttackTime` | Event/Continuous | Attack time of current sound event |
| `decay` | `StrongDecay` | Continuous | Decay characteristics |

---

## Event Classification Summary

**Discrete Events (emitted at specific moments):**
`track.start`, `track.end`, `beat`, `downbeat`, `tempo.change`, `onset`, `key.change`, `chord.change`, `pitch.change`, `loudness.peak`, `dynamic.change`, `silence.start`, `silence.end`, `gap`, `segment.boundary`, `fade.in`, `fade.out`, `click`, `discontinuity`, `noise.burst`, `saturation`, `hum`, `timbre.change`

**Continuous Signals (emitted per-frame or at regular intervals):**
`track.position`, `chroma`, `pitch`, `melody`, `loudness`, `energy`, `spectral.*`, `mfcc`, `bands.*`, `hfc`, `envelope`, `onset.rate`, `novelty`, `dissonance`, `inharmonicity`

---

## Open Questions for Next Phase

1. **Event format**: JSON lines? Protobuf? Custom binary? (JSON lines is simplest for v1)
2. **Frame size / hop size**: Typical defaults are 2048/1024 samples — should this be configurable?
3. **Which events to implement first?** Suggest a tiered approach:
   - **Tier 1 (MVP):** transport, beat, onset, silence/gap, loudness, chord.change, key.change
   - **Tier 2:** pitch, melody, spectral features, tempo.change, segment boundaries
   - **Tier 3:** audio quality, envelope, frequency bands, derived events (timbre.change, etc.)
4. **Filtering**: Should the user be able to select which events to subscribe to via CLI flags?
5. **Latency**: Some algorithms (beat tracking, key detection) need lookahead buffers — how to handle reporting delay?

---

## Deliverable

This document catalogs **~50 potential events** across 12 categories, all constructible from Essentia's C++ streaming algorithms using the compiled static library. The next step is to decide on event format, prioritize the tier 1 events, and begin the C++ application architecture.
