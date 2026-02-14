# TRACKS

A C++ command-line tool that analyzes audio files and emits structured music events via UDP multicast in real time. 
Built on [Essentia](https://essentia.upf.edu/) for audio analysis and [Protocol Buffers](https://protobuf.dev/) for wire format.

TRACKS processes a music file in two phases:

1. **Analysis** — Runs Essentia's streaming algorithms to completion (faster than real time), collecting all detected events into a sorted timeline.
2. **Emission** — Walks the timeline at playback speed, sending each event as a serialized protobuf message over UDP multicast.

Any number of receivers on the local network can join the multicast group to consume events simultaneously — no coordination required.

## Installation (Binary)

Pre-built Linux x86_64 tarballs are available from [GitHub Releases](https://github.com/YOUR_USER/tracks/releases). Each tarball is self-contained with all dependencies bundled:

```bash
# Download and extract
tar xzf tracks-0.1.0-linux-x86_64.tar.gz
cd tracks-0.1.0-linux-x86_64

# Run
./tracks audio/song.mp3

# Or add to PATH
export PATH="$PWD:$PATH"
```

No system dependencies are required beyond glibc 2.28+ (any Linux distro from 2018 onwards).

## Quick Start

```bash
# Build
cd build && cmake .. && make -j$(nproc)

# Run with default events (beat + onset)
./tracks audio/song.mp3

# Run with all 44 event types
./tracks --all audio/song.mp3

# In another terminal, run the test receiver
./tracks-recv
```

## Usage

```
tracks [options] <input-file>
```

### Options

| Flag | Description |
|------|-------------|
| `-i, --input FILE` | Input audio file (WAV or MP3). Also accepted as a positional argument. |
| `-c, --config FILE` | YAML config file (default: `config/tracks-default.yaml`) |
| `-e, --events LIST` | Comma-separated event types to enable (e.g. `beat,onset,pitch`) |
| `--all` | Enable all 44 event types |
| `--primary` | Enable tier 1 events: beat, onset, silence, loudness, energy |
| `--list-events` | Print available event types and exit |
| `--multicast-group ADDR` | Multicast group (default: `239.255.0.1`) |
| `-p, --port PORT` | UDP port (default: `5000`) |
| `--ttl N` | Multicast TTL (default: `1`) |
| `--loopback BOOL` | Enable multicast loopback (default: `true`) |
| `--interface ADDR` | Outbound interface (default: `0.0.0.0`) |
| `--sample-rate N` | Analysis sample rate (default: `44100`) |
| `--frame-size N` | Analysis frame size (default: `2048`) |
| `--hop-size N` | Analysis hop size (default: `1024`) |
| `--position-interval SEC` | Seconds between `track.position` heartbeats (default: `1.0`) |
| `--continuous-interval SEC` | Minimum interval between continuous events (default: `0.1`) |

### Examples

```bash
# Beats, chords, and key detection only
tracks -e beat,chord.change,key.change audio/song.mp3

# All events on a custom multicast group and port
tracks --all --multicast-group 239.255.1.10 -p 6000 audio/song.mp3

# Override analysis parameters via YAML config
tracks -c my-config.yaml audio/song.mp3
```

## Event Types

TRACKS detects 44 event types across 12 categories. Transport events (`track.start`, `track.end`, `track.position`) are always emitted regardless of filter settings.

| Category | Events |
|----------|--------|
| **Transport** | `track.start`, `track.end`, `track.position`, `track.abort` |
| **Beat/Rhythm** | `beat`, `tempo.change`, `downbeat` |
| **Onset** | `onset`, `onset.rate`, `novelty` |
| **Tonal** | `key.change`, `chord.change`, `chroma`, `tuning`, `dissonance`, `inharmonicity` |
| **Pitch/Melody** | `pitch`, `pitch.change`, `melody` |
| **Loudness/Energy** | `loudness`, `loudness.peak`, `energy`, `dynamic.change` |
| **Silence/Gap** | `silence.start`, `silence.end`, `gap` |
| **Spectral** | `spectral.centroid`, `spectral.flux`, `spectral.complexity`, `spectral.contrast`, `spectral.rolloff`, `mfcc`, `timbre.change` |
| **Bands** | `bands.mel`, `bands.bark`, `bands.erb`, `hfc` |
| **Structure** | `segment.boundary`, `fade.in`, `fade.out` |
| **Quality** | `click`, `discontinuity`, `noise.burst`, `saturation`, `hum` |
| **Envelope** | `envelope`, `attack`, `decay` |

Events are classified as either **discrete** (emitted at specific moments, e.g. `beat`, `chord.change`, `segment.boundary`) or **continuous** (emitted per analysis frame, e.g. `loudness`, `mfcc`, `spectral.centroid`). Continuous events are throttled to the `--continuous-interval` rate to avoid flooding the network.

See [PROTOBUF.md](PROTOBUF.md) for the wire format and full message schemas. See [CLIENT.md](CLIENT.md) for guidance on writing receivers.

## Configuration

Settings are loaded in order of precedence (highest wins):

1. CLI arguments
2. YAML config file (`-c` or default `config/tracks-default.yaml`)
3. Built-in defaults

Example YAML config:

```yaml
network:
  multicast_group: "239.255.0.1"
  port: 5000
  ttl: 1
  loopback: true
  interface: "0.0.0.0"

analysis:
  sample_rate: 44100
  frame_size: 2048
  hop_size: 1024

transport:
  position_interval: 1.0
```

## Building from Source

### Prerequisites

- C++17 compiler (GCC 8+ or Clang 7+)
- CMake 3.14+
- [Essentia](https://essentia.upf.edu/) (installed system-wide with pkg-config)
- Protocol Buffers (`protobuf-devel`)
- yaml-cpp (`yaml-cpp-devel`)
- Boost (`boost-devel` — system, program_options)

### Build

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

This produces two binaries:

- `build/tracks` — the main sender
- `build/tracks-recv` — a test receiver that joins the multicast group, decodes events, and prints them to stdout

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│  tracks (sender)                                        │
│                                                         │
│  ┌──────────┐    ┌──────────┐    ┌───────────────────┐  │
│  │ Analyzer │───>│ Timeline │───>│     Emitter       │  │
│  │ (Essentia│    │ (sorted  │    │ (real-time clock  │  │
│  │ streaming│    │  events) │    │  + sleep_until)   │  │
│  │ network) │    │          │    │        │          │  │
│  └──────────┘    └──────────┘    │        ▼          │  │
│                                  │   ┌──────────┐   │  │
│                                  │   │Transport │   │  │
│                                  │   │(UDP mcast)│   │  │
│                                  │   └────┬─────┘   │  │
│                                  └────────┼─────────┘  │
└───────────────────────────────────────────┼─────────────┘
                                            │ protobuf
                                            ▼ UDP multicast
                               ┌────────────────────────┐
                               │  tracks-recv / clients  │
                               └────────────────────────┘
```

### Source Layout

```
src/
  main.cpp        CLI entry point
  config.h/.cpp   YAML + CLI config loading
  analyzer.h/.cpp Essentia streaming pipeline (multi-pass)
  emitter.h/.cpp  Real-time timeline playback
  transport.h/.cpp UDP multicast sender (Boost.Asio)
  events.h/.cpp   Event types, names, filters, timeline
proto/
  tracks.proto    Protobuf message definitions
recv/
  main.cpp        Test receiver
config/
  tracks-default.yaml  Default configuration
```

## License

GPL 3.0 https://github.com/davesmith10/tracks?tab=GPL-3.0-1-ov-file
