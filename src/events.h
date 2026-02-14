#pragma once

#include <string>
#include <vector>
#include <set>
#include <unordered_map>

namespace tracks {

// All supported event types
enum class EventType {
    // Transport
    TRACK_START,
    TRACK_END,
    TRACK_POSITION,
    TRACK_ABORT,

    // Beat/Rhythm
    BEAT,
    TEMPO_CHANGE,
    DOWNBEAT,

    // Onset
    ONSET,
    ONSET_RATE,
    NOVELTY,

    // Tonal
    KEY_CHANGE,
    CHORD_CHANGE,
    CHROMA,
    TUNING,
    DISSONANCE,
    INHARMONICITY,

    // Pitch/Melody
    PITCH,
    PITCH_CHANGE,
    MELODY,

    // Loudness/Energy
    LOUDNESS,
    LOUDNESS_PEAK,
    ENERGY,
    DYNAMIC_CHANGE,

    // Silence/Gap
    SILENCE_START,
    SILENCE_END,
    GAP,

    // Spectral
    SPECTRAL_CENTROID,
    SPECTRAL_FLUX,
    SPECTRAL_COMPLEXITY,
    SPECTRAL_CONTRAST,
    SPECTRAL_ROLLOFF,
    MFCC,
    TIMBRE_CHANGE,

    // Bands
    BANDS_MEL,
    BANDS_BARK,
    BANDS_ERB,
    HFC,

    // Structure
    SEGMENT_BOUNDARY,
    FADE_IN,
    FADE_OUT,

    // Quality
    CLICK,
    DISCONTINUITY,
    NOISE_BURST,
    SATURATION,
    HUM,

    // Envelope/Transient
    ENVELOPE,
    ATTACK,
    DECAY,
};

using EventFilter = std::set<EventType>;

// Name-to-enum mapping (lowercase dotted names like "beat", "key.change")
const std::unordered_map<std::string, EventType>& event_name_map();

// Enum-to-name mapping
const std::string& event_type_name(EventType et);

// Predefined filter sets
EventFilter default_events();   // beat + onset (backward compatible)
EventFilter tier1_events();     // beat, onset, silence, loudness, energy
EventFilter tier2_events();     // tier1 + spectral, tonal, pitch, melody, segmentation
EventFilter all_events();       // everything (excluding transport, which is always on)

// Transport events are always emitted regardless of filter
bool is_transport_event(EventType et);

// Parse comma-separated event names into a filter
// Returns empty set on error, prints message to stderr
EventFilter parse_event_filter(const std::string& csv);

// --- Timeline ---

struct TimelineEvent {
    double      timestamp;   // seconds from start of file
    std::string serialized;  // serialized protobuf Envelope
};

using Timeline = std::vector<TimelineEvent>;

} // namespace tracks
