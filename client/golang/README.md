# TRACKS Go Receiver

A Go client that receives real-time audio analysis events from the TRACKS sender over UDP multicast.

## Build

```bash
cd client/golang
go build -o tracks-recv-go .
```

## Usage

```bash
./tracks-recv-go [flags]
```

### Flags

| Flag | Default | Description |
|------|---------|-------------|
| `-multicast-group` | `239.255.0.1` | Multicast group address to join |
| `-port` | `5000` | UDP port to listen on |
| `-interface` | `0.0.0.0` | Network interface address to bind to |

### Example

Start the receiver in one terminal:

```bash
./tracks-recv-go
```

Then run the TRACKS sender in another:

```bash
./build/tracks audio/test.mp3
```

The receiver prints each event as it arrives in real time:

```
TRACKS Receiver (Go) - listening on 239.255.0.1:5000
Waiting for events...

[   0.000] track.start       file=audio/test.mp3 duration=30.52s sr=44100 ch=2
[   0.523] beat              confidence=0.842
[   1.045] beat              confidence=0.791
[   2.500] chord.change      chord=Am strength=0.910
[  30.520] track.end

Track ended.
```

The receiver exits automatically on `track.end` or `track.abort`. Press Ctrl+C to stop it manually.

## Protobuf Bindings

The generated file `trackspb/tracks.pb.go` is committed so you don't need `protoc` installed. To regenerate it from `proto/tracks.proto`:

```bash
protoc --go_out=trackspb --go_opt=paths=source_relative \
  --go_opt=Mtracks.proto=github.com/davesmith10/tracks/client/golang/trackspb \
  -I ../../proto ../../proto/tracks.proto
```
