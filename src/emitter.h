#pragma once

#include "events.h"
#include "transport.h"
#include <atomic>

namespace tracks {

// Global interrupt flag — set by signal handler
extern std::atomic<bool> g_interrupted;

class Emitter {
public:
    // Plays back the timeline in real-time, sending each event via transport.
    // If g_interrupted becomes true, sends TrackAbort and returns.
    void run(const Timeline& timeline, Transport& transport);
};

} // namespace tracks
