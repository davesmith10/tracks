#pragma once

#include "miniaudio.h"
#include <atomic>
#include <string>

class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();

    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    // Decode file and configure device, but don't start playback yet.
    bool prepare(const std::string& path);

    // Begin playback.
    bool start();

    // Stop playback and release resources.
    void stop();

    // Current playback position in seconds.
    double get_position_seconds() const;

    // Total duration in seconds (available after prepare).
    double get_duration_seconds() const;

    // True when decoder has reached end of file.
    bool is_finished() const;

    // True when a file is prepared or playing.
    bool is_active() const;

private:
    static void data_callback(ma_device* device, void* output,
                               const void* input, ma_uint32 frame_count);

    ma_decoder decoder_{};
    ma_device device_{};
    bool decoder_initialized_ = false;
    bool device_initialized_ = false;

    std::atomic<ma_uint64> frames_played_{0};
    std::atomic<bool> finished_{false};
    ma_uint32 sample_rate_ = 0;
    ma_uint64 total_frames_ = 0;
};
