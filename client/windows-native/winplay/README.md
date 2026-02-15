# WINPLAY

A Windows command-line audio player that works in tandem with [TRACKS](../TRACKS), a WSL2-based Music Information Retrieval (MIR) tool. TRACKS analyses an audio file and emits events over UDP; WINPLAY listens for those events and plays the corresponding audio on the Windows host so that playback is synchronised with the analysis stream.

## How It Works

1. Start `winplay.exe` — it binds a UDP socket and waits.
2. Launch TRACKS inside WSL2 against an audio file.
3. TRACKS sends a `TrackPrepare` message containing the file path and a countdown.
4. WINPLAY translates the WSL2 path to a Windows path, opens the file with **miniaudio**, and reports ready.
5. On `TrackStart`, playback begins. A progress indicator is shown in the console.
6. On `TrackEnd` or `TrackAbort`, playback stops and the process exits.

Ctrl+C is handled gracefully at any point.

## Prerequisites

- Windows 11 with WSL2
- MSVC Build Tools 2022 (cl.exe, link.exe)
- CMake 3.20+
- Ninja
- vcpkg (with `VCPKG_ROOT` set and on PATH)

See [DEVTOOLS-README.md](DEVTOOLS-README.md) for full toolchain details.

## Building

From a Developer Command Prompt (or after running `C:\dev\devenv.cmd`):

```cmd
cd WINPLAY
mkdir build && cd build
cmake -G Ninja ^
  -DCMAKE_C_COMPILER=cl ^
  -DCMAKE_CXX_COMPILER=cl ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake ^
  ..
ninja
```

This produces `winplay.exe` in the build directory.

## Usage

```
winplay.exe [--port PORT] [--distro DISTRO]
```

| Option | Default | Description |
|--------|---------|-------------|
| `--port`, `-p` | 5000 | UDP port to listen on |
| `--distro` | Ubuntu | WSL2 distro name (used for path translation) |
| `--help`, `-h` | | Show help |

### Example

```cmd
winplay.exe --port 5000
```

Then, in WSL2:

```bash
tracks /path/to/song.mp3
```

## Project Structure

```
WINPLAY/
├── CMakeLists.txt
├── vcpkg.json              # vcpkg manifest (protobuf)
├── proto/
│   └── tracks.proto        # shared protobuf schema
├── include/
│   └── miniaudio.h         # single-header audio library
└── src/
    ├── main.cpp            # entry point and state machine
    ├── udp_receiver.cpp    # Winsock UDP listener
    ├── audio_player.cpp    # miniaudio playback wrapper
    ├── path_translator.cpp # WSL2-to-Windows path conversion
    └── console_status.cpp  # console progress display
```

## Dependencies

| Library | Purpose | Managed by |
|---------|---------|------------|
| protobuf | Deserialise TRACKS event messages | vcpkg |
| miniaudio | Audio playback | vendored header (`include/miniaudio.h`) |
| Winsock2 (ws2_32) | UDP networking | Windows SDK |
