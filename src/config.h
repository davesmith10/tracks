#pragma once

#include "events.h"
#include <string>
#include <cstdint>

namespace tracks {

struct Config {
    // network
    std::string multicast_group = "239.255.0.1";
    uint16_t    port            = 5000;
    int         ttl             = 1;
    bool        loopback        = true;
    std::string interface       = "0.0.0.0";

    // analysis
    int    sample_rate = 44100;
    int    frame_size  = 2048;
    int    hop_size    = 1024;

    // transport
    double position_interval = 1.0;

    // event filtering
    EventFilter enabled_events;         // which non-transport events to analyze/emit
    double      continuous_interval = 0.1; // seconds between continuous event emissions

    // input
    std::string input_file;
};

// Load config: YAML file first, then CLI args override.
// Returns true on success, false on error (e.g. --help requested).
bool load_config(Config& cfg, int argc, char* argv[]);

} // namespace tracks
