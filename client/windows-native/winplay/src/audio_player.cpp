#define MINIAUDIO_IMPLEMENTATION
#include "audio_player.h"
#include <cstdio>

AudioPlayer::AudioPlayer() = default;

AudioPlayer::~AudioPlayer() {
    stop();
}

void AudioPlayer::data_callback(ma_device* device, void* output,
                                 const void* /*input*/, ma_uint32 frame_count) {
    auto* player = static_cast<AudioPlayer*>(device->pUserData);
    if (player->finished_.load(std::memory_order_relaxed)) {
        // Fill silence once finished
        ma_uint32 bytes = frame_count * ma_get_bytes_per_frame(
            device->playback.format, device->playback.channels);
        memset(output, 0, bytes);
        return;
    }

    ma_uint64 frames_read = 0;
    ma_decoder_read_pcm_frames(&player->decoder_, output, frame_count, &frames_read);

    player->frames_played_.fetch_add(frames_read, std::memory_order_relaxed);

    if (frames_read < frame_count) {
        // Fill remaining with silence
        ma_uint32 bytes_per_frame = ma_get_bytes_per_frame(
            device->playback.format, device->playback.channels);
        memset(static_cast<char*>(output) + frames_read * bytes_per_frame,
               0, (frame_count - frames_read) * bytes_per_frame);
        player->finished_.store(true, std::memory_order_relaxed);
    }
}

bool AudioPlayer::prepare(const std::string& path) {
    stop();  // clean up any previous playback

    ma_decoder_config decoder_config = ma_decoder_config_init(
        ma_format_f32, 2, 0);  // stereo float32, native sample rate

    ma_result result = ma_decoder_init_file(path.c_str(), &decoder_config, &decoder_);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "Error: failed to open audio file: %s (error %d)\n",
                path.c_str(), result);
        return false;
    }
    decoder_initialized_ = true;
    sample_rate_ = decoder_.outputSampleRate;

    // Get total length
    ma_decoder_get_length_in_pcm_frames(&decoder_, &total_frames_);

    // Configure playback device
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = decoder_.outputFormat;
    config.playback.channels = decoder_.outputChannels;
    config.sampleRate = decoder_.outputSampleRate;
    config.dataCallback = data_callback;
    config.pUserData = this;

    result = ma_device_init(nullptr, &config, &device_);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "Error: failed to initialize audio device (error %d)\n", result);
        ma_decoder_uninit(&decoder_);
        decoder_initialized_ = false;
        return false;
    }
    device_initialized_ = true;

    frames_played_.store(0, std::memory_order_relaxed);
    finished_.store(false, std::memory_order_relaxed);

    return true;
}

bool AudioPlayer::start() {
    if (!device_initialized_) return false;

    ma_result result = ma_device_start(&device_);
    if (result != MA_SUCCESS) {
        fprintf(stderr, "Error: failed to start audio device (error %d)\n", result);
        return false;
    }
    return true;
}

void AudioPlayer::stop() {
    if (device_initialized_) {
        ma_device_uninit(&device_);
        device_initialized_ = false;
    }
    if (decoder_initialized_) {
        ma_decoder_uninit(&decoder_);
        decoder_initialized_ = false;
    }
    frames_played_.store(0, std::memory_order_relaxed);
    finished_.store(false, std::memory_order_relaxed);
    sample_rate_ = 0;
    total_frames_ = 0;
}

double AudioPlayer::get_position_seconds() const {
    if (sample_rate_ == 0) return 0.0;
    return static_cast<double>(frames_played_.load(std::memory_order_relaxed))
           / static_cast<double>(sample_rate_);
}

double AudioPlayer::get_duration_seconds() const {
    if (sample_rate_ == 0) return 0.0;
    return static_cast<double>(total_frames_) / static_cast<double>(sample_rate_);
}

bool AudioPlayer::is_finished() const {
    return finished_.load(std::memory_order_relaxed);
}

bool AudioPlayer::is_active() const {
    return decoder_initialized_;
}
