#include "console_status.h"
#include <algorithm>
#include <cstdio>

void ConsoleStatus::finish_progress_line() {
    if (on_progress_line_) {
        fprintf(stdout, "\n");
        on_progress_line_ = false;
    }
}

void ConsoleStatus::print_banner(uint16_t port) {
    fprintf(stdout, "WINPLAY v0.1.0 - listening on port %u\n", port);
    fflush(stdout);
}

void ConsoleStatus::print_prepare(const std::string& filename, double countdown) {
    finish_progress_line();
    fprintf(stdout, "[PREPARE] %s (starting in %.1fs)\n", filename.c_str(), countdown);
    fflush(stdout);
}

void ConsoleStatus::update_playing(const std::string& filename,
                                    double position, double duration) {
    std::string pos_str = format_time(position);
    std::string dur_str = format_time(duration);
    std::string bar = make_progress_bar(
        duration > 0.0 ? position / duration : 0.0);

    fprintf(stdout, "\r[PLAYING] %s  %s / %s  %s",
            filename.c_str(), pos_str.c_str(), dur_str.c_str(), bar.c_str());
    fflush(stdout);
    on_progress_line_ = true;
}

void ConsoleStatus::print_ended() {
    finish_progress_line();
    fprintf(stdout, "[ENDED]   Playback complete.\n");
    fflush(stdout);
}

void ConsoleStatus::print_aborted(const std::string& reason) {
    finish_progress_line();
    fprintf(stdout, "[ABORT]   Playback aborted");
    if (!reason.empty()) {
        fprintf(stdout, " (%s)", reason.c_str());
    }
    fprintf(stdout, "\n");
    fflush(stdout);
}

void ConsoleStatus::print_shutdown() {
    finish_progress_line();
    fprintf(stdout, "[STOP]    Shutting down.\n");
    fflush(stdout);
}

std::string ConsoleStatus::format_time(double seconds) {
    if (seconds < 0.0) seconds = 0.0;
    int total = static_cast<int>(seconds);
    int min = total / 60;
    int sec = total % 60;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", min, sec);
    return buf;
}

std::string ConsoleStatus::make_progress_bar(double fraction, int width) {
    fraction = std::clamp(fraction, 0.0, 1.0);
    int filled = static_cast<int>(fraction * width);
    std::string bar(filled, '=');
    bar.append(width - filled, '-');
    return bar;
}
