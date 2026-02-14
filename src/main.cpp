#include "config.h"
#include "analyzer.h"
#include "emitter.h"
#include "transport.h"

#include <essentia/algorithmfactory.h>
#include <iostream>
#include <csignal>

static void signal_handler(int) {
    tracks::g_interrupted.store(true, std::memory_order_relaxed);
}

int main(int argc, char* argv[]) {
    tracks::Config cfg;
    if (!tracks::load_config(cfg, argc, argv)) {
        return 1;
    }

    std::cout << "TRACKS - Audio Event Emitter" << std::endl;
    std::cout << "Input: " << cfg.input_file << std::endl;
    std::cout << "Multicast: " << cfg.multicast_group << ":" << cfg.port << std::endl;
    std::cout << "Events: " << cfg.enabled_events.size() << " types enabled" << std::endl;

    // Install signal handlers
    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    essentia::init();

    // Phase 1: Analyze
    std::cout << "\n--- Analysis Phase ---" << std::endl;
    auto timeline = tracks::analyze(cfg);

    if (tracks::g_interrupted.load()) {
        std::cout << "\nInterrupted during analysis." << std::endl;
        essentia::shutdown();
        return 130;
    }

    // Phase 2: Emit in real-time
    std::cout << "\n--- Emission Phase ---" << std::endl;
    tracks::Transport transport(cfg);
    tracks::Emitter emitter;
    emitter.run(timeline, transport, cfg);

    if (tracks::g_interrupted.load()) {
        std::cout << "Aborted." << std::endl;
        essentia::shutdown();
        return 130;
    }

    std::cout << "\nDone." << std::endl;
    essentia::shutdown();
    return 0;
}
