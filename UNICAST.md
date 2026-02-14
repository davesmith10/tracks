# Unicast Mode (WSL2 Workaround)

## The Problem

WSL2 runs inside a Hyper-V virtual machine with its own virtual network adapter. The Hyper-V virtual switch does not relay IGMP joins or forward multicast traffic between the VM and the Windows host. This means a receiver running on Windows cannot join the multicast group and will never see packets sent by TRACKS inside WSL2.

## The Solution

TRACKS has a built-in `--enable-unicast` flag that sends each packet to **both** the multicast group and a unicast UDP endpoint. Receivers on the Windows host can listen on the unicast port directly, bypassing the multicast limitation entirely.

This is a dual-send approach — every call to `Transport::send()` emits two copies of the packet: one to the multicast group (for any LAN receivers) and one to the unicast target (for the Windows host). There is no relay thread or external tool required.

## Usage

### Auto-detect (recommended)

```bash
tracks --enable-unicast audio/song.mp3
```

When no target is specified, TRACKS auto-detects the Windows host IP by parsing the default gateway from `ip route show default`. In WSL2 this is always the Windows host (typically `172.x.x.x`). On startup you will see:

```
Unicast enabled: also sending to 172.18.224.1:5000
```

### Explicit target

If auto-detection fails or you want to send to a different machine:

```bash
tracks --enable-unicast --unicast-target 192.168.1.100 audio/song.mp3
```

### YAML config

You can also set these in your YAML config file:

```yaml
network:
  enable_unicast: true
  unicast_target: ""          # empty = auto-detect
  # unicast_target: "192.168.1.100"  # or specify an IP
```

## Receiving on Windows

On the Windows host, run the test receiver or any UDP listener on the configured port (default 5000):

```bash
# Using the bundled test receiver
tracks-recv.exe

# Or with netcat / ncat
ncat -u -l 5000
```

The receiver does not need to join any multicast group — packets arrive as plain unicast UDP.

## How Auto-Detection Works

In WSL2, the Windows host is always the default gateway. TRACKS runs:

```
ip route show default
```

This returns output like:

```
default via 172.18.224.1 dev eth0 proto kernel
```

TRACKS extracts the IP address after `via` and uses it as the unicast target. If the command fails or produces unexpected output, a warning is printed and unicast is not enabled. Use `--unicast-target` to specify the IP manually in that case.

## Notes

- Unicast mode sends duplicate traffic — one multicast packet and one unicast packet per event. On a local loopback interface this has negligible overhead.
- The unicast endpoint uses the same port as the multicast group (configured via `--port` or `network.port` in YAML).
- This feature is not WSL2-specific. You can use `--enable-unicast --unicast-target <ip>` to send a unicast copy to any host on the network.
