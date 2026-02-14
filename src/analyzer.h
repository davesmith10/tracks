#pragma once

#include "config.h"
#include "events.h"
#include <string>

namespace tracks {

// Runs Essentia streaming analysis on an audio file.
// Only runs passes for event types present in cfg.enabled_events.
// Returns a sorted timeline of serialized protobuf Envelope events.
Timeline analyze(const Config& cfg);

} // namespace tracks
