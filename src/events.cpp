#include "events.h"
#include <iostream>
#include <sstream>

namespace tracks {

struct EventNameEntry {
    EventType   type;
    std::string name;
};

static const std::vector<EventNameEntry>& all_entries() {
    static const std::vector<EventNameEntry> entries = {
        // Transport
        {EventType::TRACK_START,    "track.start"},
        {EventType::TRACK_END,      "track.end"},
        {EventType::TRACK_POSITION, "track.position"},
        {EventType::TRACK_ABORT,    "track.abort"},
        // Beat/Rhythm
        {EventType::BEAT,           "beat"},
        {EventType::TEMPO_CHANGE,   "tempo.change"},
        {EventType::DOWNBEAT,       "downbeat"},
        // Onset
        {EventType::ONSET,          "onset"},
        {EventType::ONSET_RATE,     "onset.rate"},
        {EventType::NOVELTY,        "novelty"},
        // Tonal
        {EventType::KEY_CHANGE,     "key.change"},
        {EventType::CHORD_CHANGE,   "chord.change"},
        {EventType::CHROMA,         "chroma"},
        {EventType::TUNING,         "tuning"},
        {EventType::DISSONANCE,     "dissonance"},
        {EventType::INHARMONICITY,  "inharmonicity"},
        // Pitch/Melody
        {EventType::PITCH,          "pitch"},
        {EventType::PITCH_CHANGE,   "pitch.change"},
        {EventType::MELODY,         "melody"},
        // Loudness/Energy
        {EventType::LOUDNESS,       "loudness"},
        {EventType::LOUDNESS_PEAK,  "loudness.peak"},
        {EventType::ENERGY,         "energy"},
        {EventType::DYNAMIC_CHANGE, "dynamic.change"},
        // Silence/Gap
        {EventType::SILENCE_START,  "silence.start"},
        {EventType::SILENCE_END,    "silence.end"},
        {EventType::GAP,            "gap"},
        // Spectral
        {EventType::SPECTRAL_CENTROID,    "spectral.centroid"},
        {EventType::SPECTRAL_FLUX,        "spectral.flux"},
        {EventType::SPECTRAL_COMPLEXITY,  "spectral.complexity"},
        {EventType::SPECTRAL_CONTRAST,    "spectral.contrast"},
        {EventType::SPECTRAL_ROLLOFF,     "spectral.rolloff"},
        {EventType::MFCC,                 "mfcc"},
        {EventType::TIMBRE_CHANGE,        "timbre.change"},
        // Bands
        {EventType::BANDS_MEL,      "bands.mel"},
        {EventType::BANDS_BARK,     "bands.bark"},
        {EventType::BANDS_ERB,      "bands.erb"},
        {EventType::HFC,            "hfc"},
        // Structure
        {EventType::SEGMENT_BOUNDARY, "segment.boundary"},
        {EventType::FADE_IN,          "fade.in"},
        {EventType::FADE_OUT,         "fade.out"},
        // Quality
        {EventType::CLICK,           "click"},
        {EventType::DISCONTINUITY,   "discontinuity"},
        {EventType::NOISE_BURST,     "noise.burst"},
        {EventType::SATURATION,      "saturation"},
        {EventType::HUM,             "hum"},
        // Envelope/Transient
        {EventType::ENVELOPE,        "envelope"},
        {EventType::ATTACK,          "attack"},
        {EventType::DECAY,           "decay"},
    };
    return entries;
}

const std::unordered_map<std::string, EventType>& event_name_map() {
    static std::unordered_map<std::string, EventType> map;
    if (map.empty()) {
        for (const auto& e : all_entries()) {
            map[e.name] = e.type;
        }
    }
    return map;
}

const std::string& event_type_name(EventType et) {
    static std::unordered_map<EventType, const std::string*> reverse;
    if (reverse.empty()) {
        for (const auto& e : all_entries()) {
            reverse[e.type] = &e.name;
        }
    }
    static const std::string unknown = "unknown";
    auto it = reverse.find(et);
    return (it != reverse.end()) ? *it->second : unknown;
}

bool is_transport_event(EventType et) {
    return et == EventType::TRACK_START ||
           et == EventType::TRACK_END   ||
           et == EventType::TRACK_POSITION ||
           et == EventType::TRACK_ABORT;
}

EventFilter default_events() {
    return {EventType::BEAT, EventType::ONSET};
}

EventFilter tier1_events() {
    return {
        EventType::BEAT,
        EventType::ONSET,
        EventType::SILENCE_START,
        EventType::SILENCE_END,
        EventType::GAP,
        EventType::LOUDNESS,
        EventType::LOUDNESS_PEAK,
        EventType::ENERGY,
        EventType::DYNAMIC_CHANGE,
    };
}

EventFilter tier2_events() {
    auto filter = tier1_events();
    // Add tonal
    filter.insert(EventType::KEY_CHANGE);
    filter.insert(EventType::CHORD_CHANGE);
    filter.insert(EventType::CHROMA);
    filter.insert(EventType::TUNING);
    filter.insert(EventType::DISSONANCE);
    filter.insert(EventType::INHARMONICITY);
    // Add pitch/melody
    filter.insert(EventType::PITCH);
    filter.insert(EventType::PITCH_CHANGE);
    filter.insert(EventType::MELODY);
    // Add rhythm extras
    filter.insert(EventType::TEMPO_CHANGE);
    filter.insert(EventType::DOWNBEAT);
    filter.insert(EventType::ONSET_RATE);
    filter.insert(EventType::NOVELTY);
    // Add spectral
    filter.insert(EventType::SPECTRAL_CENTROID);
    filter.insert(EventType::SPECTRAL_FLUX);
    filter.insert(EventType::SPECTRAL_COMPLEXITY);
    filter.insert(EventType::SPECTRAL_CONTRAST);
    filter.insert(EventType::SPECTRAL_ROLLOFF);
    filter.insert(EventType::MFCC);
    filter.insert(EventType::TIMBRE_CHANGE);
    // Add bands
    filter.insert(EventType::BANDS_MEL);
    filter.insert(EventType::BANDS_BARK);
    filter.insert(EventType::BANDS_ERB);
    filter.insert(EventType::HFC);
    // Add structure
    filter.insert(EventType::SEGMENT_BOUNDARY);
    filter.insert(EventType::FADE_IN);
    filter.insert(EventType::FADE_OUT);
    return filter;
}

EventFilter all_events() {
    EventFilter filter;
    for (const auto& e : all_entries()) {
        if (!is_transport_event(e.type)) {
            filter.insert(e.type);
        }
    }
    return filter;
}

EventFilter parse_event_filter(const std::string& csv) {
    EventFilter filter;
    const auto& names = event_name_map();
    std::istringstream stream(csv);
    std::string token;

    while (std::getline(stream, token, ',')) {
        // Trim whitespace
        size_t start = token.find_first_not_of(" \t");
        size_t end   = token.find_last_not_of(" \t");
        if (start == std::string::npos) continue;
        token = token.substr(start, end - start + 1);

        auto it = names.find(token);
        if (it == names.end()) {
            std::cerr << "Warning: unknown event type '" << token << "', skipping\n";
            continue;
        }
        if (is_transport_event(it->second)) {
            std::cerr << "Warning: transport event '" << token
                      << "' is always enabled, skipping\n";
            continue;
        }
        filter.insert(it->second);
    }
    return filter;
}

} // namespace tracks
