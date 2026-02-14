#include "tracks.pb.h"

#include <boost/asio.hpp>
#include <boost/program_options.hpp>
#include <iostream>
#include <sstream>
#include <array>

namespace po = boost::program_options;
using boost::asio::ip::udp;
using boost::asio::ip::address;

static std::string format_floats(const google::protobuf::RepeatedField<float>& vals, int max_show = 4) {
    std::ostringstream os;
    os << "[";
    int n = vals.size();
    for (int i = 0; i < n && i < max_show; ++i) {
        if (i > 0) os << ",";
        char buf[16];
        snprintf(buf, sizeof(buf), "%.3f", vals[i]);
        os << buf;
    }
    if (n > max_show) os << ",..." << n << " total";
    os << "]";
    return os.str();
}

static std::string format_event(const tracks::Envelope& env) {
    std::string result;
    char buf[256];

    snprintf(buf, sizeof(buf), "[%8.3f] ", env.timestamp());
    result += buf;

    switch (env.event_case()) {
        // Transport
        case tracks::Envelope::kTrackStart: {
            const auto& e = env.track_start();
            snprintf(buf, sizeof(buf), "track.start       file=%s duration=%.2fs sr=%d ch=%d",
                     e.filename().c_str(), e.duration(), e.sample_rate(), e.channels());
            result += buf;
            break;
        }
        case tracks::Envelope::kTrackEnd:
            result += "track.end";
            break;
        case tracks::Envelope::kTrackPosition: {
            const auto& e = env.track_position();
            snprintf(buf, sizeof(buf), "track.position    pos=%.3fs", e.position());
            result += buf;
            break;
        }
        case tracks::Envelope::kTrackAbort: {
            const auto& e = env.track_abort();
            snprintf(buf, sizeof(buf), "track.abort       reason=%s", e.reason().c_str());
            result += buf;
            break;
        }

        // Beat/Rhythm
        case tracks::Envelope::kBeat: {
            const auto& e = env.beat();
            snprintf(buf, sizeof(buf), "beat              confidence=%.3f", e.confidence());
            result += buf;
            break;
        }
        case tracks::Envelope::kTempoChange: {
            const auto& e = env.tempo_change();
            snprintf(buf, sizeof(buf), "tempo.change      bpm=%.1f", e.bpm());
            result += buf;
            break;
        }
        case tracks::Envelope::kDownbeat: {
            const auto& e = env.downbeat();
            snprintf(buf, sizeof(buf), "downbeat          confidence=%.3f", e.confidence());
            result += buf;
            break;
        }

        // Onset
        case tracks::Envelope::kOnset: {
            const auto& e = env.onset();
            snprintf(buf, sizeof(buf), "onset             strength=%.3f", e.strength());
            result += buf;
            break;
        }
        case tracks::Envelope::kOnsetRate: {
            const auto& e = env.onset_rate();
            snprintf(buf, sizeof(buf), "onset.rate        rate=%.2f/s", e.rate());
            result += buf;
            break;
        }
        case tracks::Envelope::kNovelty: {
            const auto& e = env.novelty();
            snprintf(buf, sizeof(buf), "novelty           value=%.4f", e.value());
            result += buf;
            break;
        }

        // Tonal
        case tracks::Envelope::kKeyChange: {
            const auto& e = env.key_change();
            snprintf(buf, sizeof(buf), "key.change        key=%s scale=%s strength=%.3f",
                     e.key().c_str(), e.scale().c_str(), e.strength());
            result += buf;
            break;
        }
        case tracks::Envelope::kChordChange: {
            const auto& e = env.chord_change();
            snprintf(buf, sizeof(buf), "chord.change      chord=%s strength=%.3f",
                     e.chord().c_str(), e.strength());
            result += buf;
            break;
        }
        case tracks::Envelope::kChroma:
            result += "chroma            values=" + format_floats(env.chroma().values());
            break;
        case tracks::Envelope::kTuning: {
            const auto& e = env.tuning();
            snprintf(buf, sizeof(buf), "tuning            freq=%.2fHz", e.frequency());
            result += buf;
            break;
        }
        case tracks::Envelope::kDissonance: {
            const auto& e = env.dissonance();
            snprintf(buf, sizeof(buf), "dissonance        value=%.4f", e.value());
            result += buf;
            break;
        }
        case tracks::Envelope::kInharmonicity: {
            const auto& e = env.inharmonicity();
            snprintf(buf, sizeof(buf), "inharmonicity     value=%.4f", e.value());
            result += buf;
            break;
        }

        // Pitch/Melody
        case tracks::Envelope::kPitch: {
            const auto& e = env.pitch();
            snprintf(buf, sizeof(buf), "pitch             freq=%.1fHz confidence=%.3f",
                     e.frequency(), e.confidence());
            result += buf;
            break;
        }
        case tracks::Envelope::kPitchChange: {
            const auto& e = env.pitch_change();
            snprintf(buf, sizeof(buf), "pitch.change      from=%.1fHz to=%.1fHz",
                     e.from_hz(), e.to_hz());
            result += buf;
            break;
        }
        case tracks::Envelope::kMelody: {
            const auto& e = env.melody();
            snprintf(buf, sizeof(buf), "melody            freq=%.1fHz", e.frequency());
            result += buf;
            break;
        }

        // Loudness/Energy
        case tracks::Envelope::kLoudness: {
            const auto& e = env.loudness();
            snprintf(buf, sizeof(buf), "loudness          value=%.2f", e.value());
            result += buf;
            break;
        }
        case tracks::Envelope::kLoudnessPeak: {
            const auto& e = env.loudness_peak();
            snprintf(buf, sizeof(buf), "loudness.peak     value=%.2f", e.value());
            result += buf;
            break;
        }
        case tracks::Envelope::kEnergy: {
            const auto& e = env.energy();
            snprintf(buf, sizeof(buf), "energy            value=%.4f", e.value());
            result += buf;
            break;
        }
        case tracks::Envelope::kDynamicChange: {
            const auto& e = env.dynamic_change();
            snprintf(buf, sizeof(buf), "dynamic.change    magnitude=%.3f", e.magnitude());
            result += buf;
            break;
        }

        // Silence/Gap
        case tracks::Envelope::kSilenceStart:
            result += "silence.start";
            break;
        case tracks::Envelope::kSilenceEnd:
            result += "silence.end";
            break;
        case tracks::Envelope::kGap: {
            const auto& e = env.gap();
            snprintf(buf, sizeof(buf), "gap               duration=%.3fs", e.duration());
            result += buf;
            break;
        }

        // Spectral
        case tracks::Envelope::kSpectralCentroid: {
            const auto& e = env.spectral_centroid();
            snprintf(buf, sizeof(buf), "spectral.centroid value=%.1f", e.value());
            result += buf;
            break;
        }
        case tracks::Envelope::kSpectralFlux: {
            const auto& e = env.spectral_flux();
            snprintf(buf, sizeof(buf), "spectral.flux     value=%.4f", e.value());
            result += buf;
            break;
        }
        case tracks::Envelope::kSpectralComplexity: {
            const auto& e = env.spectral_complexity();
            snprintf(buf, sizeof(buf), "spectral.complex  value=%.4f", e.value());
            result += buf;
            break;
        }
        case tracks::Envelope::kSpectralContrast:
            result += "spectral.contrast values=" + format_floats(env.spectral_contrast().values());
            break;
        case tracks::Envelope::kSpectralRolloff: {
            const auto& e = env.spectral_rolloff();
            snprintf(buf, sizeof(buf), "spectral.rolloff  value=%.1fHz", e.value());
            result += buf;
            break;
        }
        case tracks::Envelope::kMfcc:
            result += "mfcc              values=" + format_floats(env.mfcc().values());
            break;
        case tracks::Envelope::kTimbreChange: {
            const auto& e = env.timbre_change();
            snprintf(buf, sizeof(buf), "timbre.change     distance=%.4f", e.distance());
            result += buf;
            break;
        }

        // Bands
        case tracks::Envelope::kBandsMel:
            result += "bands.mel         values=" + format_floats(env.bands_mel().values());
            break;
        case tracks::Envelope::kBandsBark:
            result += "bands.bark        values=" + format_floats(env.bands_bark().values());
            break;
        case tracks::Envelope::kBandsErb:
            result += "bands.erb         values=" + format_floats(env.bands_erb().values());
            break;
        case tracks::Envelope::kHfc: {
            const auto& e = env.hfc();
            snprintf(buf, sizeof(buf), "hfc               value=%.4f", e.value());
            result += buf;
            break;
        }

        // Structure
        case tracks::Envelope::kSegmentBoundary:
            result += "segment.boundary";
            break;
        case tracks::Envelope::kFadeIn: {
            const auto& e = env.fade_in();
            snprintf(buf, sizeof(buf), "fade.in           end=%.3fs", e.end_time());
            result += buf;
            break;
        }
        case tracks::Envelope::kFadeOut: {
            const auto& e = env.fade_out();
            snprintf(buf, sizeof(buf), "fade.out          start=%.3fs", e.start_time());
            result += buf;
            break;
        }

        // Quality
        case tracks::Envelope::kClick:
            result += "click";
            break;
        case tracks::Envelope::kDiscontinuity:
            result += "discontinuity";
            break;
        case tracks::Envelope::kNoiseBurst:
            result += "noise.burst";
            break;
        case tracks::Envelope::kSaturation: {
            const auto& e = env.saturation();
            snprintf(buf, sizeof(buf), "saturation        duration=%.3fs", e.duration());
            result += buf;
            break;
        }
        case tracks::Envelope::kHum: {
            const auto& e = env.hum();
            snprintf(buf, sizeof(buf), "hum               freq=%.1fHz", e.frequency());
            result += buf;
            break;
        }

        // Envelope/Transient
        case tracks::Envelope::kEnvelopeEvent: {
            const auto& e = env.envelope_event();
            snprintf(buf, sizeof(buf), "envelope          value=%.4f", e.value());
            result += buf;
            break;
        }
        case tracks::Envelope::kAttack: {
            const auto& e = env.attack();
            snprintf(buf, sizeof(buf), "attack            log_time=%.4f", e.log_attack_time());
            result += buf;
            break;
        }
        case tracks::Envelope::kDecay: {
            const auto& e = env.decay();
            snprintf(buf, sizeof(buf), "decay             value=%.4f", e.value());
            result += buf;
            break;
        }

        default:
            result += "unknown";
            break;
    }
    return result;
}

int main(int argc, char* argv[]) {
    std::string multicast_group = "239.255.0.1";
    uint16_t port = 5000;
    std::string listen_addr = "0.0.0.0";

    po::options_description desc("TRACKS Receiver");
    desc.add_options()
        ("help,h", "Show help")
        ("multicast-group", po::value<std::string>(&multicast_group), "Multicast group address")
        ("port,p", po::value<uint16_t>(&port), "UDP port")
        ("interface", po::value<std::string>(&listen_addr), "Listen interface address")
    ;

    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    } catch (const po::error& e) {
        std::cerr << "Error: " << e.what() << "\n" << desc << "\n";
        return 1;
    }
    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 0;
    }

    std::cout << "TRACKS Receiver - listening on " << multicast_group << ":" << port << std::endl;

    boost::asio::io_context io;

    // Create socket and bind to the multicast port
    udp::endpoint listen_endpoint(address::from_string(listen_addr), port);
    udp::socket socket(io, listen_endpoint);

    // Join the multicast group
    socket.set_option(boost::asio::ip::multicast::join_group(
        address::from_string(multicast_group)));

    std::cout << "Waiting for events...\n" << std::endl;

    std::array<char, 65536> recv_buf;
    for (;;) {
        udp::endpoint sender;
        boost::system::error_code ec;
        size_t len = socket.receive_from(boost::asio::buffer(recv_buf), sender, 0, ec);
        if (ec) {
            std::cerr << "recv error: " << ec.message() << std::endl;
            continue;
        }

        tracks::Envelope env;
        if (!env.ParseFromArray(recv_buf.data(), len)) {
            std::cerr << "failed to parse envelope (" << len << " bytes)" << std::endl;
            continue;
        }

        std::cout << format_event(env) << std::endl;

        // Exit after track.end or track.abort
        if (env.event_case() == tracks::Envelope::kTrackEnd) {
            std::cout << "\nTrack ended." << std::endl;
            break;
        }
        if (env.event_case() == tracks::Envelope::kTrackAbort) {
            std::cout << "\nTrack aborted." << std::endl;
            break;
        }
    }

    return 0;
}
