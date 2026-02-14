#include "analyzer.h"
#include "tracks.pb.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>

#include <essentia/algorithmfactory.h>
#include <essentia/streaming/algorithms/poolstorage.h>
#include <essentia/scheduler/network.h>
#include <essentia/utils/tnt/tnt2vector.h>

using namespace essentia;
using namespace essentia::streaming;
using namespace essentia::scheduler;

namespace tracks {

// --- Helpers ---

static double frame_to_time(int frame_idx, int hop_size, int sample_rate) {
    return static_cast<double>(frame_idx) * hop_size / sample_rate;
}

static void add_envelope(Timeline& tl, double ts, const ::tracks::Envelope& env) {
    TimelineEvent te;
    te.timestamp = ts;
    te.serialized = env.SerializeAsString();
    tl.push_back(te);
}

static bool needs_any(const EventFilter& filter, std::initializer_list<EventType> types) {
    for (auto t : types) {
        if (filter.count(t)) return true;
    }
    return false;
}

// --- Pass: Beat tracking ---

static double run_beat_pass(const Config& cfg, Pool& pool) {
    auto& factory = streaming::AlgorithmFactory::instance();

    Algorithm* loader = factory.create("MonoLoader",
        "filename", cfg.input_file,
        "sampleRate", Real(cfg.sample_rate));

    Algorithm* beatTracker = factory.create("BeatTrackerMultiFeature");

    loader->output("audio")           >> beatTracker->input("signal");
    beatTracker->output("ticks")      >> PC(pool, "rhythm.ticks");
    beatTracker->output("confidence") >> PC(pool, "rhythm.confidence");

    std::cout << "  Analyzing beats..." << std::endl;
    Network network(loader);
    network.run();

    long totalSamples = loader->output("audio").totalProduced();
    return static_cast<double>(totalSamples) / cfg.sample_rate;
}

// --- Pass: Onset detection ---

static void run_onset_pass(const Config& cfg, Pool& pool) {
    auto& factory = streaming::AlgorithmFactory::instance();

    Algorithm* loader = factory.create("MonoLoader",
        "filename", cfg.input_file,
        "sampleRate", Real(cfg.sample_rate));

    Algorithm* onsetRate = factory.create("OnsetRate");

    loader->output("audio")           >> onsetRate->input("signal");
    onsetRate->output("onsetTimes")   >> PC(pool, "rhythm.onsetTimes");
    onsetRate->output("onsetRate")    >> NOWHERE;

    std::cout << "  Analyzing onsets..." << std::endl;
    Network network(loader);
    network.run();
}

// --- Pass: Silence detection ---

static void run_silence_pass(const Config& cfg, Pool& pool) {
    auto& factory = streaming::AlgorithmFactory::instance();

    Algorithm* loader = factory.create("MonoLoader",
        "filename", cfg.input_file,
        "sampleRate", Real(cfg.sample_rate));

    Algorithm* frameCutter = factory.create("FrameCutter",
        "frameSize", cfg.frame_size,
        "hopSize", cfg.hop_size,
        "silentFrames", std::string("keep"));

    Algorithm* silence = factory.create("StartStopSilence",
        "threshold", -60);

    loader->output("audio")        >> frameCutter->input("signal");
    frameCutter->output("frame")   >> silence->input("frame");
    silence->output("startFrame")  >> PC(pool, "silence.startFrame");
    silence->output("stopFrame")   >> PC(pool, "silence.stopFrame");

    std::cout << "  Analyzing silence..." << std::endl;
    Network network(loader);
    network.run();
}

// --- Pass: Loudness & Energy (frame-level) ---

static void run_loudness_energy_pass(const Config& cfg, Pool& pool) {
    auto& factory = streaming::AlgorithmFactory::instance();

    Algorithm* loader = factory.create("MonoLoader",
        "filename", cfg.input_file,
        "sampleRate", Real(cfg.sample_rate));

    Algorithm* frameCutter = factory.create("FrameCutter",
        "frameSize", cfg.frame_size,
        "hopSize", cfg.hop_size,
        "silentFrames", std::string("keep"));

    Algorithm* loudness = factory.create("Loudness");
    Algorithm* energy   = factory.create("Energy");

    loader->output("audio")        >> frameCutter->input("signal");
    frameCutter->output("frame")   >> loudness->input("signal");
    frameCutter->output("frame")   >> energy->input("array");
    loudness->output("loudness")   >> PC(pool, "loudness.values");
    energy->output("energy")       >> PC(pool, "energy.values");

    std::cout << "  Analyzing loudness & energy..." << std::endl;
    Network network(loader);
    network.run();
}

// --- Pass: Spectral analysis (big combined pass) ---
// MonoLoader -> FrameCutter -> Windowing -> Spectrum
// Then fan out to: MFCC, MelBands, BarkBands, ERBBands,
//   SpectralComplexity, SpectralContrast, Flux, RollOff, HFC,
//   SpectralPeaks -> HPCP -> Key + ChordsDetection
//   SpectralPeaks -> Dissonance, Inharmonicity
//   PitchYinFFT
// Also: SpectralCentroidTime from frames (time-domain)

static void run_spectral_pass(const Config& cfg, Pool& pool, const EventFilter& filter) {
    auto& factory = streaming::AlgorithmFactory::instance();

    int spectrumSize = cfg.frame_size / 2 + 1;

    Algorithm* loader = factory.create("MonoLoader",
        "filename", cfg.input_file,
        "sampleRate", Real(cfg.sample_rate));

    Algorithm* frameCutter = factory.create("FrameCutter",
        "frameSize", cfg.frame_size,
        "hopSize", cfg.hop_size,
        "silentFrames", std::string("noise"));

    Algorithm* windowing = factory.create("Windowing",
        "type", std::string("hann"));

    Algorithm* spectrum = factory.create("Spectrum");

    loader->output("audio")      >> frameCutter->input("signal");
    frameCutter->output("frame") >> windowing->input("frame");
    windowing->output("frame")   >> spectrum->input("frame");

    // SpectralCentroidTime operates on time-domain frames
    bool want_centroid = filter.count(EventType::SPECTRAL_CENTROID);
    Algorithm* centroid = nullptr;
    if (want_centroid) {
        centroid = factory.create("SpectralCentroidTime",
            "sampleRate", Real(cfg.sample_rate));
        frameCutter->output("frame") >> centroid->input("array");
        centroid->output("centroid")  >> PC(pool, "spectral.centroid");
    }

    // --- Spectrum consumers ---

    // MFCC (also used for timbre change detection and segmentation features)
    bool want_mfcc = needs_any(filter, {EventType::MFCC, EventType::TIMBRE_CHANGE,
                                         EventType::SEGMENT_BOUNDARY});
    Algorithm* mfcc = nullptr;
    if (want_mfcc) {
        mfcc = factory.create("MFCC",
            "inputSize", spectrumSize,
            "sampleRate", Real(cfg.sample_rate));
        spectrum->output("spectrum") >> mfcc->input("spectrum");
        mfcc->output("mfcc")  >> PC(pool, "spectral.mfcc");
        mfcc->output("bands") >> NOWHERE;
    }

    // MelBands
    bool want_mel = filter.count(EventType::BANDS_MEL);
    Algorithm* melBands = nullptr;
    if (want_mel) {
        melBands = factory.create("MelBands",
            "inputSize", spectrumSize,
            "sampleRate", Real(cfg.sample_rate));
        spectrum->output("spectrum") >> melBands->input("spectrum");
        melBands->output("bands")    >> PC(pool, "bands.mel");
    }

    // BarkBands
    bool want_bark = filter.count(EventType::BANDS_BARK);
    Algorithm* barkBands = nullptr;
    if (want_bark) {
        barkBands = factory.create("BarkBands",
            "sampleRate", Real(cfg.sample_rate));
        spectrum->output("spectrum") >> barkBands->input("spectrum");
        barkBands->output("bands")   >> PC(pool, "bands.bark");
    }

    // ERBBands
    bool want_erb = filter.count(EventType::BANDS_ERB);
    Algorithm* erbBands = nullptr;
    if (want_erb) {
        erbBands = factory.create("ERBBands",
            "inputSize", spectrumSize,
            "sampleRate", Real(cfg.sample_rate));
        spectrum->output("spectrum") >> erbBands->input("spectrum");
        erbBands->output("bands")    >> PC(pool, "bands.erb");
    }

    // SpectralComplexity
    bool want_complexity = filter.count(EventType::SPECTRAL_COMPLEXITY);
    Algorithm* complexity = nullptr;
    if (want_complexity) {
        complexity = factory.create("SpectralComplexity",
            "sampleRate", Real(cfg.sample_rate));
        spectrum->output("spectrum")         >> complexity->input("spectrum");
        complexity->output("spectralComplexity") >> PC(pool, "spectral.complexity");
    }

    // SpectralContrast
    bool want_contrast = filter.count(EventType::SPECTRAL_CONTRAST);
    Algorithm* contrast = nullptr;
    if (want_contrast) {
        contrast = factory.create("SpectralContrast",
            "sampleRate", Real(cfg.sample_rate));
        spectrum->output("spectrum")            >> contrast->input("spectrum");
        contrast->output("spectralContrast")    >> PC(pool, "spectral.contrast");
        contrast->output("spectralValley")      >> NOWHERE;
    }

    // Flux (SpectralFlux)
    bool want_flux = filter.count(EventType::SPECTRAL_FLUX);
    Algorithm* flux = nullptr;
    if (want_flux) {
        flux = factory.create("Flux");
        spectrum->output("spectrum") >> flux->input("spectrum");
        flux->output("flux")         >> PC(pool, "spectral.flux");
    }

    // RollOff
    bool want_rolloff = filter.count(EventType::SPECTRAL_ROLLOFF);
    Algorithm* rolloff = nullptr;
    if (want_rolloff) {
        rolloff = factory.create("RollOff",
            "sampleRate", Real(cfg.sample_rate));
        spectrum->output("spectrum") >> rolloff->input("spectrum");
        rolloff->output("rollOff")   >> PC(pool, "spectral.rolloff");
    }

    // HFC
    bool want_hfc = filter.count(EventType::HFC);
    Algorithm* hfcAlgo = nullptr;
    if (want_hfc) {
        hfcAlgo = factory.create("HFC",
            "sampleRate", Real(cfg.sample_rate));
        spectrum->output("spectrum") >> hfcAlgo->input("spectrum");
        hfcAlgo->output("hfc")       >> PC(pool, "spectral.hfc");
    }

    // SpectralPeaks — needed for HPCP, Dissonance, Inharmonicity
    // Note: Dissonance and Inharmonicity crash on 0 Hz peaks, so we use a
    // separate SpectralPeaks with minFrequency for those.
    bool want_hpcp = needs_any(filter, {EventType::CHROMA, EventType::KEY_CHANGE,
                                         EventType::CHORD_CHANGE, EventType::TUNING});
    bool want_diss_inharm = needs_any(filter, {EventType::DISSONANCE, EventType::INHARMONICITY});
    bool want_peaks = want_hpcp || want_diss_inharm;

    if (want_peaks) {
        Algorithm* spectralPeaks = factory.create("SpectralPeaks",
            "sampleRate", Real(cfg.sample_rate));
        spectrum->output("spectrum") >> spectralPeaks->input("spectrum");

        if (want_hpcp) {
            Algorithm* hpcp = factory.create("HPCP");
            spectralPeaks->output("frequencies") >> hpcp->input("frequencies");
            spectralPeaks->output("magnitudes")  >> hpcp->input("magnitudes");
            hpcp->output("hpcp")                 >> PC(pool, "tonal.hpcp");

            // Key (streaming composite — accumulates HPCPs internally)
            if (needs_any(filter, {EventType::KEY_CHANGE})) {
                Algorithm* key = factory.create("Key");
                hpcp->output("hpcp")    >> key->input("pcp");
                key->output("key")      >> PC(pool, "tonal.key");
                key->output("scale")    >> PC(pool, "tonal.scale");
                key->output("strength") >> PC(pool, "tonal.keyStrength");
            }

            // ChordsDetection
            if (filter.count(EventType::CHORD_CHANGE)) {
                Algorithm* chords = factory.create("ChordsDetection",
                    "sampleRate", Real(cfg.sample_rate),
                    "hopSize", cfg.hop_size);
                hpcp->output("hpcp")         >> chords->input("pcp");
                chords->output("chords")     >> PC(pool, "tonal.chords");
                chords->output("strength")   >> PC(pool, "tonal.chordStrength");
            }
        }

        if (!want_hpcp) {
            // Still need to sink the peaks outputs if HPCP doesn't consume them
            spectralPeaks->output("frequencies") >> NOWHERE;
            spectralPeaks->output("magnitudes")  >> NOWHERE;
        }

        // Dissonance & Inharmonicity use a filtered SpectralPeaks (minFrequency > 0)
        if (want_diss_inharm) {
            Algorithm* filteredPeaks = factory.create("SpectralPeaks",
                "sampleRate", Real(cfg.sample_rate),
                "minFrequency", Real(20.0));
            spectrum->output("spectrum") >> filteredPeaks->input("spectrum");

            if (filter.count(EventType::DISSONANCE)) {
                Algorithm* diss = factory.create("Dissonance");
                filteredPeaks->output("frequencies") >> diss->input("frequencies");
                filteredPeaks->output("magnitudes")  >> diss->input("magnitudes");
                diss->output("dissonance")           >> PC(pool, "tonal.dissonance");
            }

            if (filter.count(EventType::INHARMONICITY)) {
                Algorithm* inharm = factory.create("Inharmonicity");
                filteredPeaks->output("frequencies") >> inharm->input("frequencies");
                filteredPeaks->output("magnitudes")  >> inharm->input("magnitudes");
                inharm->output("inharmonicity")      >> PC(pool, "tonal.inharmonicity");
            }

        }
    }

    // PitchYinFFT
    bool want_pitch = needs_any(filter, {EventType::PITCH, EventType::PITCH_CHANGE});
    Algorithm* pitchYin = nullptr;
    if (want_pitch) {
        pitchYin = factory.create("PitchYinFFT",
            "sampleRate", Real(cfg.sample_rate));
        spectrum->output("spectrum")        >> pitchYin->input("spectrum");
        pitchYin->output("pitch")           >> PC(pool, "pitch.values");
        pitchYin->output("pitchConfidence") >> PC(pool, "pitch.confidence");
    }

    // If spectrum has no consumers at all, sink it
    bool spectrum_has_consumer = want_mfcc || want_mel || want_bark || want_erb ||
        want_complexity || want_contrast || want_flux || want_rolloff || want_hfc ||
        want_peaks || want_pitch || want_diss_inharm;
    if (!spectrum_has_consumer) {
        spectrum->output("spectrum") >> NOWHERE;
    }

    std::cout << "  Analyzing spectral features..." << std::endl;
    Network network(loader);
    network.run();
}

// --- Pass: Melody (PredominantPitchMelodia) ---

static void run_melody_pass(const Config& cfg, Pool& pool) {
    auto& factory = streaming::AlgorithmFactory::instance();

    Algorithm* loader = factory.create("MonoLoader",
        "filename", cfg.input_file,
        "sampleRate", Real(cfg.sample_rate));

    Algorithm* melody = factory.create("PredominantPitchMelodia",
        "sampleRate", Real(cfg.sample_rate),
        "frameSize", cfg.frame_size,
        "hopSize", cfg.hop_size);

    loader->output("audio")              >> melody->input("signal");
    melody->output("pitch")              >> PC(pool, "melody.pitch");
    melody->output("pitchConfidence")    >> PC(pool, "melody.confidence");

    std::cout << "  Analyzing melody..." << std::endl;
    Network network(loader);
    network.run();
}

// --- Build timeline from pool data ---

static void build_beat_events(const Pool& pool, const EventFilter& filter, Timeline& tl) {
    if (!filter.count(EventType::BEAT)) return;
    if (!pool.contains<std::vector<Real>>("rhythm.ticks")) return;

    const auto& ticks = pool.value<std::vector<Real>>("rhythm.ticks");
    std::vector<Real> confidences;
    if (pool.contains<std::vector<Real>>("rhythm.confidence")) {
        confidences = pool.value<std::vector<Real>>("rhythm.confidence");
    }

    for (size_t i = 0; i < ticks.size(); ++i) {
        ::tracks::Envelope env;
        double t = static_cast<double>(ticks[i]);
        env.set_timestamp(t);
        auto* beat = env.mutable_beat();
        if (i < confidences.size()) {
            beat->set_confidence(static_cast<double>(confidences[i]));
        }
        add_envelope(tl, t, env);
    }
    std::cout << "    " << ticks.size() << " beats" << std::endl;
}

static void build_onset_events(const Pool& pool, const EventFilter& filter, Timeline& tl) {
    if (!filter.count(EventType::ONSET)) return;
    if (!pool.contains<std::vector<Real>>("rhythm.onsetTimes")) return;

    const auto& onsets = pool.value<std::vector<Real>>("rhythm.onsetTimes");
    for (size_t i = 0; i < onsets.size(); ++i) {
        ::tracks::Envelope env;
        double t = static_cast<double>(onsets[i]);
        env.set_timestamp(t);
        env.mutable_onset()->set_strength(1.0);
        add_envelope(tl, t, env);
    }
    std::cout << "    " << onsets.size() << " onsets" << std::endl;
}

static void build_silence_events(const Pool& pool, const EventFilter& filter,
                                  const Config& cfg, double duration, Timeline& tl) {
    if (!needs_any(filter, {EventType::SILENCE_START, EventType::SILENCE_END, EventType::GAP}))
        return;
    if (!pool.contains<std::vector<Real>>("silence.startFrame") ||
        !pool.contains<std::vector<Real>>("silence.stopFrame"))
        return;

    const auto& starts = pool.value<std::vector<Real>>("silence.startFrame");
    const auto& stops  = pool.value<std::vector<Real>>("silence.stopFrame");
    if (starts.empty() || stops.empty()) return;

    int startFrame = static_cast<int>(starts.back());
    int stopFrame  = static_cast<int>(stops.back());
    double startTime = frame_to_time(startFrame, cfg.hop_size, cfg.sample_rate);
    double stopTime  = frame_to_time(stopFrame, cfg.hop_size, cfg.sample_rate);
    int silence_count = 0;

    if (startFrame > 0 && startTime > 0.05) {
        if (filter.count(EventType::SILENCE_START)) {
            ::tracks::Envelope env; env.set_timestamp(0.0);
            env.mutable_silence_start();
            add_envelope(tl, 0.0, env); silence_count++;
        }
        if (filter.count(EventType::SILENCE_END)) {
            ::tracks::Envelope env; env.set_timestamp(startTime);
            env.mutable_silence_end();
            add_envelope(tl, startTime, env); silence_count++;
        }
        if (filter.count(EventType::GAP)) {
            ::tracks::Envelope env; env.set_timestamp(0.0);
            env.mutable_gap()->set_duration(startTime);
            add_envelope(tl, 0.0, env); silence_count++;
        }
    }

    if (stopTime < duration - 0.05) {
        if (filter.count(EventType::SILENCE_START)) {
            ::tracks::Envelope env; env.set_timestamp(stopTime);
            env.mutable_silence_start();
            add_envelope(tl, stopTime, env); silence_count++;
        }
        if (filter.count(EventType::SILENCE_END)) {
            ::tracks::Envelope env; env.set_timestamp(duration);
            env.mutable_silence_end();
            add_envelope(tl, duration, env); silence_count++;
        }
        if (filter.count(EventType::GAP)) {
            ::tracks::Envelope env; env.set_timestamp(stopTime);
            env.mutable_gap()->set_duration(duration - stopTime);
            add_envelope(tl, stopTime, env); silence_count++;
        }
    }

    std::cout << "    " << silence_count << " silence events"
              << " (start=" << startFrame << " stop=" << stopFrame << ")" << std::endl;
}

static void build_loudness_events(const Pool& pool, const EventFilter& filter,
                                   const Config& cfg, double duration, Timeline& tl) {
    bool want_loudness = filter.count(EventType::LOUDNESS);
    bool want_peak     = filter.count(EventType::LOUDNESS_PEAK);
    bool want_dynamic  = filter.count(EventType::DYNAMIC_CHANGE);
    if (!want_loudness && !want_peak && !want_dynamic) return;
    if (!pool.contains<std::vector<Real>>("loudness.values")) return;

    const auto& values = pool.value<std::vector<Real>>("loudness.values");
    if (values.empty()) return;

    double interval = cfg.continuous_interval;
    double last_emit_time = -interval;
    int loudness_count = 0, peak_count = 0, dynamic_count = 0;

    Real max_loudness = *std::max_element(values.begin(), values.end());
    Real peak_threshold = max_loudness * 0.9f;

    for (size_t i = 0; i < values.size(); ++i) {
        double t = frame_to_time(static_cast<int>(i), cfg.hop_size, cfg.sample_rate);
        if (t > duration) break;

        if (want_loudness && (t - last_emit_time) >= interval) {
            ::tracks::Envelope env; env.set_timestamp(t);
            env.mutable_loudness()->set_value(static_cast<double>(values[i]));
            add_envelope(tl, t, env);
            last_emit_time = t;
            loudness_count++;
        }

        if (want_peak && i > 0 && i < values.size() - 1) {
            if (values[i] > values[i-1] && values[i] > values[i+1] &&
                values[i] >= peak_threshold) {
                ::tracks::Envelope env; env.set_timestamp(t);
                env.mutable_loudness_peak()->set_value(static_cast<double>(values[i]));
                add_envelope(tl, t, env);
                peak_count++;
            }
        }

        if (want_dynamic && i > 0) {
            double diff = std::abs(static_cast<double>(values[i]) - static_cast<double>(values[i-1]));
            double mag_thresh = static_cast<double>(max_loudness) * 0.3;
            if (mag_thresh > 0.0 && diff > mag_thresh) {
                ::tracks::Envelope env; env.set_timestamp(t);
                env.mutable_dynamic_change()->set_magnitude(diff);
                add_envelope(tl, t, env);
                dynamic_count++;
            }
        }
    }

    if (loudness_count > 0) std::cout << "    " << loudness_count << " loudness" << std::endl;
    if (peak_count > 0)     std::cout << "    " << peak_count << " loudness peaks" << std::endl;
    if (dynamic_count > 0)  std::cout << "    " << dynamic_count << " dynamic changes" << std::endl;
}

static void build_energy_events(const Pool& pool, const EventFilter& filter,
                                 const Config& cfg, double duration, Timeline& tl) {
    if (!filter.count(EventType::ENERGY)) return;
    if (!pool.contains<std::vector<Real>>("energy.values")) return;

    const auto& values = pool.value<std::vector<Real>>("energy.values");
    if (values.empty()) return;

    double interval = cfg.continuous_interval;
    double last_emit_time = -interval;
    int count = 0;

    for (size_t i = 0; i < values.size(); ++i) {
        double t = frame_to_time(static_cast<int>(i), cfg.hop_size, cfg.sample_rate);
        if (t > duration) break;
        if ((t - last_emit_time) >= interval) {
            ::tracks::Envelope env; env.set_timestamp(t);
            env.mutable_energy()->set_value(static_cast<double>(values[i]));
            add_envelope(tl, t, env);
            last_emit_time = t;
            count++;
        }
    }
    std::cout << "    " << count << " energy" << std::endl;
}

// --- Spectral continuous event builders (throttled) ---

static void build_throttled_real_events(const Pool& pool, const std::string& pool_key,
                                         const Config& cfg, double duration, Timeline& tl,
                                         const char* label,
                                         std::function<void(::tracks::Envelope&, double)> setter) {
    if (!pool.contains<std::vector<Real>>(pool_key)) return;
    const auto& values = pool.value<std::vector<Real>>(pool_key);
    if (values.empty()) return;

    double interval = cfg.continuous_interval;
    double last_emit = -interval;
    int count = 0;

    for (size_t i = 0; i < values.size(); ++i) {
        double t = frame_to_time(static_cast<int>(i), cfg.hop_size, cfg.sample_rate);
        if (t > duration) break;
        if ((t - last_emit) >= interval) {
            ::tracks::Envelope env; env.set_timestamp(t);
            setter(env, static_cast<double>(values[i]));
            add_envelope(tl, t, env);
            last_emit = t;
            count++;
        }
    }
    std::cout << "    " << count << " " << label << std::endl;
}

static void build_throttled_vector_events(const Pool& pool, const std::string& pool_key,
                                            const Config& cfg, double duration, Timeline& tl,
                                            const char* label,
                                            std::function<void(::tracks::Envelope&, const std::vector<Real>&)> setter) {
    if (!pool.contains<std::vector<std::vector<Real>>>(pool_key)) return;
    const auto& frames = pool.value<std::vector<std::vector<Real>>>(pool_key);
    if (frames.empty()) return;

    double interval = cfg.continuous_interval;
    double last_emit = -interval;
    int count = 0;

    for (size_t i = 0; i < frames.size(); ++i) {
        double t = frame_to_time(static_cast<int>(i), cfg.hop_size, cfg.sample_rate);
        if (t > duration) break;
        if ((t - last_emit) >= interval) {
            ::tracks::Envelope env; env.set_timestamp(t);
            setter(env, frames[i]);
            add_envelope(tl, t, env);
            last_emit = t;
            count++;
        }
    }
    std::cout << "    " << count << " " << label << std::endl;
}

static void build_spectral_events(const Pool& pool, const EventFilter& filter,
                                    const Config& cfg, double duration, Timeline& tl) {
    // SpectralCentroid (Real per frame)
    if (filter.count(EventType::SPECTRAL_CENTROID)) {
        build_throttled_real_events(pool, "spectral.centroid", cfg, duration, tl,
            "spectral.centroid", [](::tracks::Envelope& env, double v) {
                env.mutable_spectral_centroid()->set_value(v);
            });
    }

    // SpectralFlux
    if (filter.count(EventType::SPECTRAL_FLUX)) {
        build_throttled_real_events(pool, "spectral.flux", cfg, duration, tl,
            "spectral.flux", [](::tracks::Envelope& env, double v) {
                env.mutable_spectral_flux()->set_value(v);
            });
    }

    // SpectralComplexity
    if (filter.count(EventType::SPECTRAL_COMPLEXITY)) {
        build_throttled_real_events(pool, "spectral.complexity", cfg, duration, tl,
            "spectral.complexity", [](::tracks::Envelope& env, double v) {
                env.mutable_spectral_complexity()->set_value(v);
            });
    }

    // SpectralContrast (vector per frame)
    if (filter.count(EventType::SPECTRAL_CONTRAST)) {
        build_throttled_vector_events(pool, "spectral.contrast", cfg, duration, tl,
            "spectral.contrast", [](::tracks::Envelope& env, const std::vector<Real>& v) {
                auto* sc = env.mutable_spectral_contrast();
                for (auto val : v) sc->add_values(val);
            });
    }

    // SpectralRolloff
    if (filter.count(EventType::SPECTRAL_ROLLOFF)) {
        build_throttled_real_events(pool, "spectral.rolloff", cfg, duration, tl,
            "spectral.rolloff", [](::tracks::Envelope& env, double v) {
                env.mutable_spectral_rolloff()->set_value(v);
            });
    }

    // HFC
    if (filter.count(EventType::HFC)) {
        build_throttled_real_events(pool, "spectral.hfc", cfg, duration, tl,
            "hfc", [](::tracks::Envelope& env, double v) {
                env.mutable_hfc()->set_value(v);
            });
    }

    // MFCC (vector per frame)
    if (filter.count(EventType::MFCC)) {
        build_throttled_vector_events(pool, "spectral.mfcc", cfg, duration, tl,
            "mfcc", [](::tracks::Envelope& env, const std::vector<Real>& v) {
                auto* m = env.mutable_mfcc();
                for (auto val : v) m->add_values(val);
            });
    }

    // TimbreChange — derived from MFCC distance between consecutive frames
    if (filter.count(EventType::TIMBRE_CHANGE)) {
        if (pool.contains<std::vector<std::vector<Real>>>("spectral.mfcc")) {
            const auto& mfccs = pool.value<std::vector<std::vector<Real>>>("spectral.mfcc");
            int count = 0;
            for (size_t i = 1; i < mfccs.size(); ++i) {
                double t = frame_to_time(static_cast<int>(i), cfg.hop_size, cfg.sample_rate);
                if (t > duration) break;

                // Euclidean distance between consecutive MFCC vectors
                double dist = 0.0;
                size_t n = std::min(mfccs[i].size(), mfccs[i-1].size());
                for (size_t j = 0; j < n; ++j) {
                    double d = static_cast<double>(mfccs[i][j] - mfccs[i-1][j]);
                    dist += d * d;
                }
                dist = std::sqrt(dist);

                // Threshold: emit when distance > 50 (tunable heuristic)
                if (dist > 50.0) {
                    ::tracks::Envelope env; env.set_timestamp(t);
                    env.mutable_timbre_change()->set_distance(dist);
                    add_envelope(tl, t, env);
                    count++;
                }
            }
            if (count > 0) std::cout << "    " << count << " timbre changes" << std::endl;
        }
    }
}

// --- Band event builders ---

static void build_band_events(const Pool& pool, const EventFilter& filter,
                                const Config& cfg, double duration, Timeline& tl) {
    if (filter.count(EventType::BANDS_MEL)) {
        build_throttled_vector_events(pool, "bands.mel", cfg, duration, tl,
            "bands.mel", [](::tracks::Envelope& env, const std::vector<Real>& v) {
                auto* b = env.mutable_bands_mel();
                for (auto val : v) b->add_values(val);
            });
    }

    if (filter.count(EventType::BANDS_BARK)) {
        build_throttled_vector_events(pool, "bands.bark", cfg, duration, tl,
            "bands.bark", [](::tracks::Envelope& env, const std::vector<Real>& v) {
                auto* b = env.mutable_bands_bark();
                for (auto val : v) b->add_values(val);
            });
    }

    if (filter.count(EventType::BANDS_ERB)) {
        build_throttled_vector_events(pool, "bands.erb", cfg, duration, tl,
            "bands.erb", [](::tracks::Envelope& env, const std::vector<Real>& v) {
                auto* b = env.mutable_bands_erb();
                for (auto val : v) b->add_values(val);
            });
    }
}

// --- Tonal event builders ---

static void build_tonal_events(const Pool& pool, const EventFilter& filter,
                                const Config& cfg, double duration, Timeline& tl) {
    // Chroma (HPCP, vector per frame)
    if (filter.count(EventType::CHROMA)) {
        build_throttled_vector_events(pool, "tonal.hpcp", cfg, duration, tl,
            "chroma", [](::tracks::Envelope& env, const std::vector<Real>& v) {
                auto* c = env.mutable_chroma();
                for (auto val : v) c->add_values(val);
            });
    }

    // Key change — the streaming Key algo outputs final key only;
    // we emit a single key.change event
    if (filter.count(EventType::KEY_CHANGE)) {
        if (pool.contains<std::vector<std::string>>("tonal.key") &&
            pool.contains<std::vector<std::string>>("tonal.scale")) {
            const auto& keys   = pool.value<std::vector<std::string>>("tonal.key");
            const auto& scales = pool.value<std::vector<std::string>>("tonal.scale");
            std::vector<Real> strengths;
            if (pool.contains<std::vector<Real>>("tonal.keyStrength")) {
                strengths = pool.value<std::vector<Real>>("tonal.keyStrength");
            }
            if (!keys.empty()) {
                ::tracks::Envelope env; env.set_timestamp(0.0);
                auto* kc = env.mutable_key_change();
                kc->set_key(keys.back());
                kc->set_scale(scales.empty() ? "" : scales.back());
                kc->set_strength(strengths.empty() ? 0.0 : static_cast<double>(strengths.back()));
                add_envelope(tl, 0.0, env);
                std::cout << "    key: " << keys.back() << " " << (scales.empty() ? "" : scales.back()) << std::endl;
            }
        }
    }

    // Chord changes — ChordsDetection outputs a chord per window
    if (filter.count(EventType::CHORD_CHANGE)) {
        if (pool.contains<std::vector<std::string>>("tonal.chords")) {
            const auto& chords = pool.value<std::vector<std::string>>("tonal.chords");
            std::vector<Real> strengths;
            if (pool.contains<std::vector<Real>>("tonal.chordStrength")) {
                strengths = pool.value<std::vector<Real>>("tonal.chordStrength");
            }

            std::string prev_chord;
            int count = 0;
            for (size_t i = 0; i < chords.size(); ++i) {
                if (chords[i] != prev_chord) {
                    double t = frame_to_time(static_cast<int>(i), cfg.hop_size, cfg.sample_rate);
                    if (t > duration) break;
                    ::tracks::Envelope env; env.set_timestamp(t);
                    auto* cc = env.mutable_chord_change();
                    cc->set_chord(chords[i]);
                    if (i < strengths.size()) {
                        cc->set_strength(static_cast<double>(strengths[i]));
                    }
                    add_envelope(tl, t, env);
                    prev_chord = chords[i];
                    count++;
                }
            }
            if (count > 0) std::cout << "    " << count << " chord changes" << std::endl;
        }
    }

    // Dissonance
    if (filter.count(EventType::DISSONANCE)) {
        build_throttled_real_events(pool, "tonal.dissonance", cfg, duration, tl,
            "dissonance", [](::tracks::Envelope& env, double v) {
                env.mutable_dissonance()->set_value(v);
            });
    }

    // Inharmonicity
    if (filter.count(EventType::INHARMONICITY)) {
        build_throttled_real_events(pool, "tonal.inharmonicity", cfg, duration, tl,
            "inharmonicity", [](::tracks::Envelope& env, double v) {
                env.mutable_inharmonicity()->set_value(v);
            });
    }
}

// --- Pitch event builders ---

static void build_pitch_events(const Pool& pool, const EventFilter& filter,
                                const Config& cfg, double duration, Timeline& tl) {
    bool want_pitch  = filter.count(EventType::PITCH);
    bool want_change = filter.count(EventType::PITCH_CHANGE);
    if (!want_pitch && !want_change) return;
    if (!pool.contains<std::vector<Real>>("pitch.values")) return;

    const auto& pitches     = pool.value<std::vector<Real>>("pitch.values");
    std::vector<Real> confs;
    if (pool.contains<std::vector<Real>>("pitch.confidence")) {
        confs = pool.value<std::vector<Real>>("pitch.confidence");
    }

    double interval = cfg.continuous_interval;
    double last_emit = -interval;
    int pitch_count = 0, change_count = 0;
    Real prev_pitch = 0.0f;

    for (size_t i = 0; i < pitches.size(); ++i) {
        double t = frame_to_time(static_cast<int>(i), cfg.hop_size, cfg.sample_rate);
        if (t > duration) break;

        Real freq = pitches[i];
        Real conf = (i < confs.size()) ? confs[i] : 0.0f;

        // Continuous pitch (throttled)
        if (want_pitch && (t - last_emit) >= interval && conf > 0.3f) {
            ::tracks::Envelope env; env.set_timestamp(t);
            auto* p = env.mutable_pitch();
            p->set_frequency(static_cast<double>(freq));
            p->set_confidence(static_cast<double>(conf));
            add_envelope(tl, t, env);
            last_emit = t;
            pitch_count++;
        }

        // Pitch change: significant jump with good confidence
        if (want_change && i > 0 && conf > 0.5f && prev_pitch > 0.0f && freq > 0.0f) {
            double ratio = static_cast<double>(freq) / static_cast<double>(prev_pitch);
            // More than a semitone change (ratio > ~1.06 or < ~0.94)
            if (ratio > 1.06 || ratio < 0.94) {
                ::tracks::Envelope env; env.set_timestamp(t);
                auto* pc = env.mutable_pitch_change();
                pc->set_from_hz(static_cast<double>(prev_pitch));
                pc->set_to_hz(static_cast<double>(freq));
                add_envelope(tl, t, env);
                change_count++;
            }
        }

        if (conf > 0.3f) prev_pitch = freq;
    }

    if (pitch_count > 0)  std::cout << "    " << pitch_count << " pitch" << std::endl;
    if (change_count > 0) std::cout << "    " << change_count << " pitch changes" << std::endl;
}

// --- Melody event builder ---

static void build_melody_events(const Pool& pool, const EventFilter& filter,
                                 const Config& cfg, double duration, Timeline& tl) {
    if (!filter.count(EventType::MELODY)) return;
    if (!pool.contains<std::vector<std::vector<Real>>>("melody.pitch")) return;

    // PredominantPitchMelodia outputs vectors (one per call to process)
    // In streaming mode, it accumulates and outputs a single vector at the end
    const auto& pitch_vecs = pool.value<std::vector<std::vector<Real>>>("melody.pitch");
    if (pitch_vecs.empty()) return;

    // Flatten — the composite outputs one big vector
    const auto& pitches = pitch_vecs[0];

    // PredominantPitchMelodia uses its own hop size (default 128)
    // We need to compute time based on that hop
    int melody_hop = cfg.hop_size;  // it uses our configured hop

    double interval = cfg.continuous_interval;
    double last_emit = -interval;
    int count = 0;

    for (size_t i = 0; i < pitches.size(); ++i) {
        double t = frame_to_time(static_cast<int>(i), melody_hop, cfg.sample_rate);
        if (t > duration) break;

        Real freq = pitches[i];
        if (freq <= 0.0f) continue;  // unvoiced

        if ((t - last_emit) >= interval) {
            ::tracks::Envelope env; env.set_timestamp(t);
            env.mutable_melody()->set_frequency(static_cast<double>(freq));
            add_envelope(tl, t, env);
            last_emit = t;
            count++;
        }
    }
    if (count > 0) std::cout << "    " << count << " melody" << std::endl;
}

// --- Segmentation event builder (SBic in standard mode from MFCC pool) ---

static void build_segmentation_events(const Pool& pool, const EventFilter& filter,
                                        const Config& cfg, double duration, Timeline& tl) {
    if (!filter.count(EventType::SEGMENT_BOUNDARY)) return;
    if (!pool.contains<std::vector<std::vector<Real>>>("spectral.mfcc")) return;

    const auto& mfccs = pool.value<std::vector<std::vector<Real>>>("spectral.mfcc");
    if (mfccs.size() < 10) return;  // need enough frames

    // Build feature matrix for SBic (standard mode)
    // SBic expects TNT::Array2D<Real> with features as rows, frames as columns
    size_t n_coeff = mfccs[0].size();
    size_t n_frames = mfccs.size();

    TNT::Array2D<Real> features(static_cast<int>(n_coeff), static_cast<int>(n_frames));
    for (size_t f = 0; f < n_frames; ++f) {
        for (size_t c = 0; c < n_coeff && c < mfccs[f].size(); ++c) {
            features[static_cast<int>(c)][static_cast<int>(f)] = mfccs[f][c];
        }
    }

    auto* sbic = essentia::standard::AlgorithmFactory::instance().create("SBic");
    sbic->configure();

    std::vector<Real> segmentation;
    sbic->input("features").set(features);
    sbic->output("segmentation").set(segmentation);
    sbic->compute();

    int count = 0;
    // SBic returns frame indices including first and last
    for (size_t i = 1; i < segmentation.size() - 1; ++i) {
        int frame = static_cast<int>(segmentation[i]);
        double t = frame_to_time(frame, cfg.hop_size, cfg.sample_rate);
        if (t > 0.0 && t < duration) {
            ::tracks::Envelope env; env.set_timestamp(t);
            env.mutable_segment_boundary();
            add_envelope(tl, t, env);
            count++;
        }
    }

    delete sbic;
    if (count > 0) std::cout << "    " << count << " segment boundaries" << std::endl;
}

// --- Main entry point ---

Timeline analyze(const Config& cfg) {
    Timeline timeline;
    Pool pool;
    double duration = 0.0;

    const auto& filter = cfg.enabled_events;

    // --- Run analysis passes (only if needed) ---

    // Beat pass
    if (needs_any(filter, {EventType::BEAT, EventType::TEMPO_CHANGE, EventType::DOWNBEAT})) {
        duration = run_beat_pass(cfg, pool);
    } else {
        // Still need duration
        auto& factory = streaming::AlgorithmFactory::instance();
        Algorithm* loader = factory.create("MonoLoader",
            "filename", cfg.input_file,
            "sampleRate", Real(cfg.sample_rate));
        loader->output("audio") >> NOWHERE;
        Network network(loader);
        network.run();
        long totalSamples = loader->output("audio").totalProduced();
        duration = static_cast<double>(totalSamples) / cfg.sample_rate;
    }

    // Onset pass
    if (needs_any(filter, {EventType::ONSET, EventType::ONSET_RATE, EventType::NOVELTY})) {
        run_onset_pass(cfg, pool);
    }

    // Silence pass
    if (needs_any(filter, {EventType::SILENCE_START, EventType::SILENCE_END, EventType::GAP})) {
        run_silence_pass(cfg, pool);
    }

    // Loudness & Energy pass
    if (needs_any(filter, {EventType::LOUDNESS, EventType::LOUDNESS_PEAK, EventType::ENERGY,
                           EventType::DYNAMIC_CHANGE})) {
        run_loudness_energy_pass(cfg, pool);
    }

    // Spectral pass (big combined pass for spectral, bands, tonal, pitch)
    bool need_spectral = needs_any(filter, {
        EventType::SPECTRAL_CENTROID, EventType::SPECTRAL_FLUX,
        EventType::SPECTRAL_COMPLEXITY, EventType::SPECTRAL_CONTRAST,
        EventType::SPECTRAL_ROLLOFF, EventType::MFCC, EventType::TIMBRE_CHANGE,
        EventType::BANDS_MEL, EventType::BANDS_BARK, EventType::BANDS_ERB, EventType::HFC,
        EventType::CHROMA, EventType::KEY_CHANGE, EventType::CHORD_CHANGE,
        EventType::TUNING, EventType::DISSONANCE, EventType::INHARMONICITY,
        EventType::PITCH, EventType::PITCH_CHANGE,
        EventType::SEGMENT_BOUNDARY});
    if (need_spectral) {
        run_spectral_pass(cfg, pool, filter);
    }

    // Melody pass
    if (filter.count(EventType::MELODY)) {
        run_melody_pass(cfg, pool);
    }

    // --- Build timeline ---
    std::cout << "  Building timeline..." << std::endl;

    // track.start at t=0
    {
        ::tracks::Envelope env;
        env.set_timestamp(0.0);
        auto* ts = env.mutable_track_start();
        ts->set_filename(cfg.input_file);
        ts->set_duration(duration);
        ts->set_sample_rate(cfg.sample_rate);
        ts->set_channels(1);
        add_envelope(timeline, 0.0, env);
    }

    // Build events from pool data
    build_beat_events(pool, filter, timeline);
    build_onset_events(pool, filter, timeline);
    build_silence_events(pool, filter, cfg, duration, timeline);
    build_loudness_events(pool, filter, cfg, duration, timeline);
    build_energy_events(pool, filter, cfg, duration, timeline);
    build_spectral_events(pool, filter, cfg, duration, timeline);
    build_band_events(pool, filter, cfg, duration, timeline);
    build_tonal_events(pool, filter, cfg, duration, timeline);
    build_pitch_events(pool, filter, cfg, duration, timeline);
    build_melody_events(pool, filter, cfg, duration, timeline);
    build_segmentation_events(pool, filter, cfg, duration, timeline);

    // track.position heartbeats
    for (double t = cfg.position_interval; t < duration; t += cfg.position_interval) {
        ::tracks::Envelope env;
        env.set_timestamp(t);
        env.mutable_track_position()->set_position(t);
        add_envelope(timeline, t, env);
    }

    // track.end
    {
        ::tracks::Envelope env;
        env.set_timestamp(duration);
        env.mutable_track_end();
        add_envelope(timeline, duration, env);
    }

    // Sort by timestamp
    std::sort(timeline.begin(), timeline.end(),
        [](const TimelineEvent& a, const TimelineEvent& b) {
            return a.timestamp < b.timestamp;
        });

    std::cout << "  Timeline: " << timeline.size() << " events over "
              << duration << "s" << std::endl;

    return timeline;
}

} // namespace tracks
