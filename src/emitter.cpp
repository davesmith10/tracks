#include "emitter.h"
#include "tracks.pb.h"
#include <chrono>
#include <thread>
#include <iostream>

namespace tracks {

std::atomic<bool> g_interrupted{false};

void Emitter::run(const Timeline& timeline, Transport& transport) {
    if (timeline.empty()) return;

    auto wall_start = std::chrono::steady_clock::now();

    for (const auto& event : timeline) {
        if (g_interrupted.load(std::memory_order_relaxed)) {
            std::cout << "\nInterrupted — sending track.abort" << std::endl;
            ::tracks::Envelope env;
            env.set_timestamp(event.timestamp);
            auto* abort = env.mutable_track_abort();
            abort->set_reason("user_interrupt");
            transport.send(env.SerializeAsString());
            return;
        }

        // Calculate when this event should fire
        auto target = wall_start + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double>(event.timestamp));

        // Sleep until it's time (check interrupt periodically for responsiveness)
        while (std::chrono::steady_clock::now() < target) {
            if (g_interrupted.load(std::memory_order_relaxed)) break;
            auto remaining = target - std::chrono::steady_clock::now();
            auto sleep_time = std::min(remaining,
                std::chrono::steady_clock::duration(std::chrono::milliseconds(100)));
            if (sleep_time.count() > 0) {
                std::this_thread::sleep_for(sleep_time);
            }
        }

        if (g_interrupted.load(std::memory_order_relaxed)) continue; // will be caught at top of loop

        // Send the event
        transport.send(event.serialized);
    }
}

} // namespace tracks
