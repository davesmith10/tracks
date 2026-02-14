# TRACKS Protocol Buffer Schema

TRACKS uses Protocol Buffers (proto3) as its wire format. Every UDP datagram contains exactly one serialized `Envelope` message.

## Envelope

The top-level message wraps every event with a timestamp and a `oneof` discriminator:

```protobuf
message Envelope {
  double timestamp = 1;  // seconds from start of audio file
  oneof event {
    // one of the event messages below
  }
}
```

The `timestamp` field is always present and represents the time position in the audio file (not wall-clock time). The `oneof event` field contains exactly one event message per envelope.

## Field Number Ranges

Field numbers in the `oneof` are organized by category for clarity and future extensibility:

| Range | Category |
|-------|----------|
| 10–19 | Transport |
| 20–29 | Beat/Rhythm |
| 30–39 | Onset |
| 40–49 | Tonal |
| 50–59 | Pitch/Melody |
| 60–69 | Loudness/Energy |
| 70–79 | Silence/Gap |
| 80–89 | Spectral |
| 90–99 | Bands |
| 100–109 | Structure |
| 110–119 | Quality |
| 120–129 | Envelope/Transient |

## Message Reference

### Transport (10–19)

These events are always emitted regardless of the event filter.

```protobuf
message TrackStart {
  string filename    = 1;  // input file path
  double duration    = 2;  // total duration in seconds
  int32  sample_rate = 3;
  int32  channels    = 4;
}

message TrackEnd {}

message TrackPosition {
  double position = 1;  // current playback position in seconds
}

message TrackAbort {
  string reason = 1;  // e.g. "user_interrupt"
}
```

`TrackStart` is always the first event (timestamp 0.0). `TrackEnd` is the last. `TrackPosition` heartbeats are emitted at the configured `position_interval` (default 1s). `TrackAbort` is sent if playback is interrupted by a signal (SIGINT/SIGTERM).

### Beat/Rhythm (20–29)

```protobuf
message Beat {
  double confidence = 1;
}

message TempoChange {
  double bpm = 1;
}

message Downbeat {
  double confidence = 1;
}
```

### Onset (30–39)

```protobuf
message Onset {
  double strength = 1;
}

message OnsetRate {
  double rate = 1;  // onsets per second
}

message Novelty {
  double value = 1;
}
```

### Tonal (40–49)

```protobuf
message KeyChange {
  string key      = 1;  // e.g. "C", "F#"
  string scale    = 2;  // "major" or "minor"
  double strength = 3;
}

message ChordChange {
  string chord    = 1;  // e.g. "Am", "G", "Bdim"
  double strength = 2;
}

message Chroma {
  repeated float values = 1;  // 12-dimensional HPCP vector
}

message Tuning {
  double frequency = 1;  // Hz, reference tuning frequency
}

message Dissonance {
  double value = 1;  // 0.0 (consonant) to 1.0 (dissonant)
}

message Inharmonicity {
  double value = 1;  // 0.0 (harmonic) to 1.0 (inharmonic)
}
```

### Pitch/Melody (50–59)

```protobuf
message Pitch {
  double frequency  = 1;  // Hz
  double confidence = 2;  // 0.0 to 1.0
}

message PitchChange {
  double from_hz = 1;
  double to_hz   = 2;
}

message Melody {
  double frequency = 1;  // Hz, predominant melody pitch
}
```

### Loudness/Energy (60–69)

```protobuf
message Loudness {
  double value = 1;  // instantaneous loudness
}

message LoudnessPeak {
  double value = 1;  // local maximum loudness
}

message Energy {
  double value = 1;  // signal energy for current frame
}

message DynamicChange {
  double magnitude = 1;  // size of dynamic shift
}
```

### Silence/Gap (70–79)

```protobuf
message SilenceStart {}

message SilenceEnd {}

message Gap {
  double duration = 1;  // gap duration in seconds
}
```

### Spectral (80–89)

```protobuf
message SpectralCentroid {
  double value = 1;  // Hz — "brightness" of sound
}

message SpectralFlux {
  double value = 1;  // rate of spectral change
}

message SpectralComplexity {
  double value = 1;
}

message SpectralContrast {
  repeated float values = 1;  // multi-band contrast vector
}

message SpectralRolloff {
  double value = 1;  // Hz
}

message Mfcc {
  repeated float values = 1;  // 13-dimensional MFCC vector
}

message TimbreChange {
  double distance = 1;  // MFCC euclidean distance from previous frame
}
```

### Bands (90–99)

```protobuf
message BandsMel {
  repeated float values = 1;  // mel-scale band energies
}

message BandsBark {
  repeated float values = 1;  // bark-scale band energies
}

message BandsErb {
  repeated float values = 1;  // ERB-scale band energies
}

message Hfc {
  double value = 1;  // high-frequency content
}
```

### Structure (100–109)

```protobuf
message SegmentBoundary {}

message FadeIn {
  double end_time = 1;  // when the fade-in ends (seconds)
}

message FadeOut {
  double start_time = 1;  // when the fade-out begins (seconds)
}
```

### Quality (110–119)

```protobuf
message Click {}

message Discontinuity {}

message NoiseBurst {}

message Saturation {
  double duration = 1;  // duration of clipped region (seconds)
}

message Hum {
  double frequency = 1;  // Hz of detected hum
}
```

### Envelope/Transient (120–129)

```protobuf
message EnvelopeEvent {
  double value = 1;  // signal envelope amplitude
}

message Attack {
  double log_attack_time = 1;  // log10 of attack time
}

message Decay {
  double value = 1;
}
```

## Parsing

Each UDP datagram is a single serialized `Envelope`. To parse:

1. Read the raw bytes from the UDP socket.
2. Deserialize as `tracks.Envelope`.
3. Switch on the `event` oneof case to determine the event type.
4. Read the `timestamp` for the time position.

See [CLIENT.md](CLIENT.md) for complete receiver examples.

## Generating Language Bindings

The schema lives at `proto/tracks.proto`. Generate bindings for your language:

```bash
# C++
protoc --cpp_out=. proto/tracks.proto

# Python
protoc --python_out=. proto/tracks.proto

# Go
protoc --go_out=. proto/tracks.proto

# JavaScript / TypeScript (via grpc-tools or protobufjs)
npx pbjs -t static-module -w es6 -o tracks.js proto/tracks.proto

# C# / .NET
protoc --csharp_out=. proto/tracks.proto

# Java
protoc --java_out=. proto/tracks.proto
```
