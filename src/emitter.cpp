#include "emitter.h"
#include "tracks.pb.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <climits>
#include <cstdlib>

namespace tracks {

std::atomic<bool> g_interrupted{false};

void Emitter::run(const Timeline& timeline, Transport& transport, const Config& cfg) {
    if (timeline.empty()) return;

    // Send track.prepare if prepare_time > 0
    if (cfg.prepare_time > 0) {
        // Resolve canonical filename
        std::string canonical = cfg.input_file;
        char* resolved = realpath(cfg.input_file.c_str(), nullptr);
        if (resolved) {
            canonical = resolved;
            free(resolved);
        }

        // Build and send track.prepare
        ::tracks::Envelope env;
        env.set_timestamp(-cfg.prepare_time);
        auto* prep = env.mutable_track_prepare();
        prep->set_countdown(cfg.prepare_time);
        prep->set_filename(canonical);
        transport.send(env.SerializeAsString());

        std::cout << "Prepare: waiting " << cfg.prepare_time << "s before playback" << std::endl;

        // Sleep for prepare_time with interrupt checking
        auto prepare_end = std::chrono::steady_clock::now() +
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(cfg.prepare_time));

        while (std::chrono::steady_clock::now() < prepare_end) {
            if (g_interrupted.load(std::memory_order_relaxed)) {
                std::cout << "\nInterrupted during prepare — sending track.abort" << std::endl;
                ::tracks::Envelope abort_env;
                abort_env.set_timestamp(-cfg.prepare_time);
                auto* abort = abort_env.mutable_track_abort();
                abort->set_reason("user_interrupt");
                transport.send(abort_env.SerializeAsString());
                return;
            }
            auto remaining = prepare_end - std::chrono::steady_clock::now();
            auto sleep_time = std::min(remaining,
                std::chrono::steady_clock::duration(std::chrono::milliseconds(100)));
            if (sleep_time.count() > 0) {
                std::this_thread::sleep_for(sleep_time);
            }
        }
    }

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
