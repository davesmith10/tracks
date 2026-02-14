# Writing a TRACKS Client

TRACKS sends events as serialized Protocol Buffer messages over UDP multicast. Any program that can join a multicast group and parse protobuf can receive events. This guide covers the essentials for building a receiver in any language.

## Network Setup

| Parameter | Default |
|-----------|---------|
| Multicast group | `239.255.0.1` |
| Port | `5000` |
| Max datagram size | ~64 KB (practical limit) |

To receive events:

1. Create a UDP socket and bind to the port.
2. Join the multicast group.
3. Read datagrams in a loop — each one is a single serialized `tracks.Envelope`.

## Protocol

Every datagram is exactly one `tracks.Envelope` protobuf message. The envelope contains:

- `timestamp` (double) — position in the audio file in seconds
- `event` (oneof) — the specific event payload

The first event is always `track.start` (timestamp 0.0) with file metadata. The last is `track.end`. If the sender is interrupted, a `track.abort` is sent instead.

Events arrive in real time — a beat at 2.5s in the audio file arrives approximately 2.5s after `track.start`. The sender uses high-resolution sleep to pace emission.

## Generating Bindings

Copy `proto/tracks.proto` into your project and generate bindings for your language. See [PROTOBUF.md](PROTOBUF.md#generating-language-bindings) for the `protoc` commands.

## Example: Python

```python
import socket
import struct
import tracks_pb2  # generated from proto/tracks.proto

MCAST_GROUP = "239.255.0.1"
PORT = 5000

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(("", PORT))

# Join multicast group
mreq = struct.pack("4sl", socket.inet_aton(MCAST_GROUP), socket.INADDR_ANY)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, mreq)

print(f"Listening on {MCAST_GROUP}:{PORT}")

while True:
    data, addr = sock.recvfrom(65536)
    env = tracks_pb2.Envelope()
    env.ParseFromString(data)

    event_type = env.WhichOneof("event")
    print(f"[{env.timestamp:8.3f}] {event_type}")

    if event_type == "track_end":
        break
```

## Example: Node.js

```javascript
const dgram = require("dgram");
const { Envelope } = require("./tracks_pb"); // generated bindings

const MCAST_GROUP = "239.255.0.1";
const PORT = 5000;

const sock = dgram.createSocket({ type: "udp4", reuseAddr: true });

sock.bind(PORT, () => {
  sock.addMembership(MCAST_GROUP);
  console.log(`Listening on ${MCAST_GROUP}:${PORT}`);
});

sock.on("message", (buf) => {
  const env = Envelope.decode(buf);
  const eventType = env.event; // oneof field name
  console.log(`[${env.timestamp.toFixed(3)}] ${eventType}`);

  if (env.trackEnd) {
    console.log("Track ended.");
    sock.close();
  }
});
```

## Example: C++ (Boost.Asio)

The included `tracks-recv` binary is a complete reference. The core loop:

```cpp
#include "tracks.pb.h"
#include <boost/asio.hpp>

using boost::asio::ip::udp;
using boost::asio::ip::address;

boost::asio::io_context io;
udp::endpoint listen_ep(address::from_string("0.0.0.0"), 5000);
udp::socket socket(io, listen_ep);

// Join multicast group
socket.set_option(boost::asio::ip::multicast::join_group(
    address::from_string("239.255.0.1")));

std::array<char, 65536> buf;
for (;;) {
    udp::endpoint sender;
    size_t len = socket.receive_from(boost::asio::buffer(buf), sender);

    tracks::Envelope env;
    env.ParseFromArray(buf.data(), len);

    switch (env.event_case()) {
        case tracks::Envelope::kBeat:
            // handle beat at env.timestamp()
            break;
        case tracks::Envelope::kChordChange:
            // env.chord_change().chord() => "Am", "G", etc.
            break;
        case tracks::Envelope::kTrackEnd:
            return;
        // ... handle other events
    }
}
```

## Design Patterns

### Selective Listening

You don't need to handle every event type. Use the `oneof` case / `WhichOneof` to ignore events you don't care about. Alternatively, configure the sender with `--events` to only emit what you need — this reduces network traffic and analysis time.

### Event-Driven Architecture

TRACKS works well as an event source for reactive systems. Common patterns:

- **Beat-synced visuals** — trigger animations on `beat` events
- **Chord display** — update UI on `chord.change`
- **Loudness meter** — drive a VU meter from `loudness` events
- **Structural navigation** — use `segment.boundary` to identify song sections
- **Audio quality monitoring** — alert on `click`, `saturation`, or `hum` events

### Handling Continuous vs. Discrete Events

Continuous events (like `loudness`, `mfcc`, `spectral.centroid`) arrive at a regular interval controlled by `--continuous-interval` (default 0.1s = 10 Hz). Discrete events (like `beat`, `chord.change`, `onset`) arrive only when detected.

If your application only needs discrete events, filter by event type and ignore continuous ones.

### Timing

Events are emitted in real time — the sender sleeps between events to match audio playback speed. If your receiver processes events faster than they arrive (the normal case), you can handle them synchronously. For slow handlers, consider buffering events in a queue and processing them in a separate thread.

The `track.position` heartbeat (default: every 1s) can be used to synchronize or verify timing.

### Multiple Receivers

UDP multicast supports any number of receivers without additional sender load. Multiple applications can independently listen to the same TRACKS stream — for example, a visualizer and a chord display running simultaneously.

### Error Handling

- If a datagram fails to parse as an `Envelope`, skip it (the sender only emits valid protobuf).
- If the stream stops unexpectedly (no `track.end` received), the sender was likely killed. Use a receive timeout to detect this.
- UDP is unreliable by design. On a local network, packet loss is extremely rare, but receivers should be tolerant of missing events.
