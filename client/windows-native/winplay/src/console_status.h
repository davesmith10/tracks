#pragma once

#include <cstdio>
#include <string>

class ConsoleStatus {
public:
    void print_banner(uint16_t port);
    void print_prepare(const std::string& filename, double countdown);
    void update_playing(const std::string& filename,
                        double position, double duration);
    void print_ended();
    void print_aborted(const std::string& reason);
    void print_shutdown();

private:
    static std::string format_time(double seconds);
    static std::string make_progress_bar(double fraction, int width = 30);
    bool on_progress_line_ = false;

    void finish_progress_line();
};
