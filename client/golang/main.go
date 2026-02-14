package main

import (
	"flag"
	"fmt"
	"net"
	"os"
	"os/signal"
	"strings"
	"syscall"

	"github.com/davesmith10/tracks/client/golang/trackspb"
	"google.golang.org/protobuf/proto"
)

func formatFloats(vals []float32, maxShow int) string {
	var b strings.Builder
	b.WriteByte('[')
	n := len(vals)
	for i := 0; i < n && i < maxShow; i++ {
		if i > 0 {
			b.WriteByte(',')
		}
		fmt.Fprintf(&b, "%.3f", vals[i])
	}
	if n > maxShow {
		fmt.Fprintf(&b, ",...%d total", n)
	}
	b.WriteByte(']')
	return b.String()
}

func formatEvent(env *trackspb.Envelope) string {
	ts := fmt.Sprintf("[%8.3f] ", env.GetTimestamp())

	switch e := env.Event.(type) {
	// Transport
	case *trackspb.Envelope_TrackStart:
		v := e.TrackStart
		return ts + fmt.Sprintf("track.start       file=%s duration=%.2fs sr=%d ch=%d",
			v.GetFilename(), v.GetDuration(), v.GetSampleRate(), v.GetChannels())
	case *trackspb.Envelope_TrackEnd:
		return ts + "track.end"
	case *trackspb.Envelope_TrackPosition:
		return ts + fmt.Sprintf("track.position    pos=%.3fs", e.TrackPosition.GetPosition())
	case *trackspb.Envelope_TrackAbort:
		return ts + fmt.Sprintf("track.abort       reason=%s", e.TrackAbort.GetReason())

	// Beat/Rhythm
	case *trackspb.Envelope_Beat:
		return ts + fmt.Sprintf("beat              confidence=%.3f", e.Beat.GetConfidence())
	case *trackspb.Envelope_TempoChange:
		return ts + fmt.Sprintf("tempo.change      bpm=%.1f", e.TempoChange.GetBpm())
	case *trackspb.Envelope_Downbeat:
		return ts + fmt.Sprintf("downbeat          confidence=%.3f", e.Downbeat.GetConfidence())

	// Onset
	case *trackspb.Envelope_Onset:
		return ts + fmt.Sprintf("onset             strength=%.3f", e.Onset.GetStrength())
	case *trackspb.Envelope_OnsetRate:
		return ts + fmt.Sprintf("onset.rate        rate=%.2f/s", e.OnsetRate.GetRate())
	case *trackspb.Envelope_Novelty:
		return ts + fmt.Sprintf("novelty           value=%.4f", e.Novelty.GetValue())

	// Tonal
	case *trackspb.Envelope_KeyChange:
		v := e.KeyChange
		return ts + fmt.Sprintf("key.change        key=%s scale=%s strength=%.3f",
			v.GetKey(), v.GetScale(), v.GetStrength())
	case *trackspb.Envelope_ChordChange:
		v := e.ChordChange
		return ts + fmt.Sprintf("chord.change      chord=%s strength=%.3f",
			v.GetChord(), v.GetStrength())
	case *trackspb.Envelope_Chroma:
		return ts + "chroma            values=" + formatFloats(e.Chroma.GetValues(), 4)
	case *trackspb.Envelope_Tuning:
		return ts + fmt.Sprintf("tuning            freq=%.2fHz", e.Tuning.GetFrequency())
	case *trackspb.Envelope_Dissonance:
		return ts + fmt.Sprintf("dissonance        value=%.4f", e.Dissonance.GetValue())
	case *trackspb.Envelope_Inharmonicity:
		return ts + fmt.Sprintf("inharmonicity     value=%.4f", e.Inharmonicity.GetValue())

	// Pitch/Melody
	case *trackspb.Envelope_Pitch:
		v := e.Pitch
		return ts + fmt.Sprintf("pitch             freq=%.1fHz confidence=%.3f",
			v.GetFrequency(), v.GetConfidence())
	case *trackspb.Envelope_PitchChange:
		v := e.PitchChange
		return ts + fmt.Sprintf("pitch.change      from=%.1fHz to=%.1fHz",
			v.GetFromHz(), v.GetToHz())
	case *trackspb.Envelope_Melody:
		return ts + fmt.Sprintf("melody            freq=%.1fHz", e.Melody.GetFrequency())

	// Loudness/Energy
	case *trackspb.Envelope_Loudness:
		return ts + fmt.Sprintf("loudness          value=%.2f", e.Loudness.GetValue())
	case *trackspb.Envelope_LoudnessPeak:
		return ts + fmt.Sprintf("loudness.peak     value=%.2f", e.LoudnessPeak.GetValue())
	case *trackspb.Envelope_Energy:
		return ts + fmt.Sprintf("energy            value=%.4f", e.Energy.GetValue())
	case *trackspb.Envelope_DynamicChange:
		return ts + fmt.Sprintf("dynamic.change    magnitude=%.3f", e.DynamicChange.GetMagnitude())

	// Silence/Gap
	case *trackspb.Envelope_SilenceStart:
		return ts + "silence.start"
	case *trackspb.Envelope_SilenceEnd:
		return ts + "silence.end"
	case *trackspb.Envelope_Gap:
		return ts + fmt.Sprintf("gap               duration=%.3fs", e.Gap.GetDuration())

	// Spectral
	case *trackspb.Envelope_SpectralCentroid:
		return ts + fmt.Sprintf("spectral.centroid value=%.1f", e.SpectralCentroid.GetValue())
	case *trackspb.Envelope_SpectralFlux:
		return ts + fmt.Sprintf("spectral.flux     value=%.4f", e.SpectralFlux.GetValue())
	case *trackspb.Envelope_SpectralComplexity:
		return ts + fmt.Sprintf("spectral.complex  value=%.4f", e.SpectralComplexity.GetValue())
	case *trackspb.Envelope_SpectralContrast:
		return ts + "spectral.contrast values=" + formatFloats(e.SpectralContrast.GetValues(), 4)
	case *trackspb.Envelope_SpectralRolloff:
		return ts + fmt.Sprintf("spectral.rolloff  value=%.1fHz", e.SpectralRolloff.GetValue())
	case *trackspb.Envelope_Mfcc:
		return ts + "mfcc              values=" + formatFloats(e.Mfcc.GetValues(), 4)
	case *trackspb.Envelope_TimbreChange:
		return ts + fmt.Sprintf("timbre.change     distance=%.4f", e.TimbreChange.GetDistance())

	// Bands
	case *trackspb.Envelope_BandsMel:
		return ts + "bands.mel         values=" + formatFloats(e.BandsMel.GetValues(), 4)
	case *trackspb.Envelope_BandsBark:
		return ts + "bands.bark        values=" + formatFloats(e.BandsBark.GetValues(), 4)
	case *trackspb.Envelope_BandsErb:
		return ts + "bands.erb         values=" + formatFloats(e.BandsErb.GetValues(), 4)
	case *trackspb.Envelope_Hfc:
		return ts + fmt.Sprintf("hfc               value=%.4f", e.Hfc.GetValue())

	// Structure
	case *trackspb.Envelope_SegmentBoundary:
		return ts + "segment.boundary"
	case *trackspb.Envelope_FadeIn:
		return ts + fmt.Sprintf("fade.in           end=%.3fs", e.FadeIn.GetEndTime())
	case *trackspb.Envelope_FadeOut:
		return ts + fmt.Sprintf("fade.out          start=%.3fs", e.FadeOut.GetStartTime())

	// Quality
	case *trackspb.Envelope_Click:
		return ts + "click"
	case *trackspb.Envelope_Discontinuity:
		return ts + "discontinuity"
	case *trackspb.Envelope_NoiseBurst:
		return ts + "noise.burst"
	case *trackspb.Envelope_Saturation:
		return ts + fmt.Sprintf("saturation        duration=%.3fs", e.Saturation.GetDuration())
	case *trackspb.Envelope_Hum:
		return ts + fmt.Sprintf("hum               freq=%.1fHz", e.Hum.GetFrequency())

	// Envelope/Transient
	case *trackspb.Envelope_EnvelopeEvent:
		return ts + fmt.Sprintf("envelope          value=%.4f", e.EnvelopeEvent.GetValue())
	case *trackspb.Envelope_Attack:
		return ts + fmt.Sprintf("attack            log_time=%.4f", e.Attack.GetLogAttackTime())
	case *trackspb.Envelope_Decay:
		return ts + fmt.Sprintf("decay             value=%.4f", e.Decay.GetValue())

	default:
		return ts + "unknown"
	}
}

func main() {
	multicastGroup := flag.String("multicast-group", "239.255.0.1", "Multicast group address")
	port := flag.Int("port", 5000, "UDP port")
	iface := flag.String("interface", "0.0.0.0", "Listen interface address")
	flag.Parse()

	fmt.Printf("TRACKS Receiver (Go) - listening on %s:%d\n", *multicastGroup, *port)

	groupAddr := net.ParseIP(*multicastGroup)
	if groupAddr == nil {
		fmt.Fprintf(os.Stderr, "Error: invalid multicast group %q\n", *multicastGroup)
		os.Exit(1)
	}

	listenAddr := &net.UDPAddr{
		IP:   net.ParseIP(*iface),
		Port: *port,
	}

	conn, err := net.ListenMulticastUDP("udp4", nil, &net.UDPAddr{
		IP:   groupAddr,
		Port: *port,
	})
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: listen: %v\n", err)
		os.Exit(1)
	}
	defer conn.Close()
	_ = listenAddr // interface binding handled by ListenMulticastUDP

	// Graceful shutdown on Ctrl+C
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, syscall.SIGINT, syscall.SIGTERM)
	go func() {
		<-sigCh
		fmt.Println("\nInterrupted.")
		conn.Close()
		os.Exit(0)
	}()

	fmt.Println("Waiting for events...\n")

	buf := make([]byte, 65536)
	for {
		n, _, err := conn.ReadFromUDP(buf)
		if err != nil {
			// conn.Close() from signal handler causes this
			break
		}

		env := &trackspb.Envelope{}
		if err := proto.Unmarshal(buf[:n], env); err != nil {
			fmt.Fprintf(os.Stderr, "failed to parse envelope (%d bytes)\n", n)
			continue
		}

		fmt.Println(formatEvent(env))

		switch env.Event.(type) {
		case *trackspb.Envelope_TrackEnd:
			fmt.Println("\nTrack ended.")
			return
		case *trackspb.Envelope_TrackAbort:
			fmt.Println("\nTrack aborted.")
			return
		}
	}
}
