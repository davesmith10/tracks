#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "udp_receiver.h"
#include "audio_player.h"
#include "path_translator.h"
#include "console_status.h"
#include "tracks.pb.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static std::atomic<bool> g_shutdown{false};

static BOOL WINAPI ctrl_handler(DWORD type) {
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT) {
        g_shutdown.store(true, std::memory_order_relaxed);
        return TRUE;
    }
    return FALSE;
}

enum class State { WAITING, PREPARED, PLAYING, STOPPED };

static std::string basename_of(const std::string& path) {
    auto pos = path.find_last_of("/\\");
    return (pos != std::string::npos) ? path.substr(pos + 1) : path;
}

int main(int argc, char* argv[]) {
    uint16_t port = 5000;
    std::string distro = "Ubuntu";

    // Simple arg parsing: --port N --distro NAME
    for (int i = 1; i < argc; ++i) {
        if ((strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "-p") == 0) && i + 1 < argc) {
            port = static_cast<uint16_t>(atoi(argv[++i]));
        } else if (strcmp(argv[i], "--distro") == 0 && i + 1 < argc) {
            distro = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fprintf(stdout,
                "Usage: winplay.exe [--port PORT] [--distro DISTRO]\n"
                "\n"
                "  --port, -p PORT    UDP listen port (default: 5000)\n"
                "  --distro DISTRO    WSL2 distro name (default: Ubuntu)\n"
                "  --help, -h         Show this help\n");
            return 0;
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            return 1;
        }
    }

    SetConsoleCtrlHandler(ctrl_handler, TRUE);

    ConsoleStatus status;
    status.print_banner(port);

    UdpReceiver receiver(port);
    AudioPlayer player;
    PathTranslator translator(distro);

    State state = State::WAITING;
    std::string current_file;
    double current_duration = 0.0;

    char buf[65536];

    while (!g_shutdown.load(std::memory_order_relaxed) && state != State::STOPPED) {
        int len = receiver.receive(buf, sizeof(buf));

        if (len > 0) {
            tracks::Envelope env;
            if (!env.ParseFromArray(buf, len)) {
                fprintf(stderr, "Warning: failed to parse envelope (%d bytes)\n", len);
                continue;
            }

            switch (env.event_case()) {
                case tracks::Envelope::kTrackPrepare: {
                    const auto& e = env.track_prepare();
                    std::string win_path = translator.translate(e.filename());
                    current_file = basename_of(e.filename());
                    status.print_prepare(current_file, e.countdown());

                    if (!player.prepare(win_path)) {
                        fprintf(stderr, "Error: failed to prepare: %s\n", win_path.c_str());
                        state = State::STOPPED;
                    } else {
                        current_duration = player.get_duration_seconds();
                        state = State::PREPARED;
                    }
                    break;
                }

                case tracks::Envelope::kTrackStart: {
                    if (state == State::PREPARED || state == State::WAITING) {
                        // If we got TrackStart without TrackPrepare, try to prepare from TrackStart info
                        if (state == State::WAITING) {
                            const auto& e = env.track_start();
                            std::string win_path = translator.translate(e.filename());
                            current_file = basename_of(e.filename());
                            current_duration = e.duration();

                            if (!player.prepare(win_path)) {
                                fprintf(stderr, "Error: failed to prepare: %s\n", win_path.c_str());
                                state = State::STOPPED;
                                break;
                            }
                        }

                        if (player.start()) {
                            state = State::PLAYING;
                        } else {
                            fprintf(stderr, "Error: failed to start playback\n");
                            state = State::STOPPED;
                        }
                    }
                    break;
                }

                case tracks::Envelope::kTrackEnd: {
                    player.stop();
                    status.print_ended();
                    state = State::STOPPED;
                    break;
                }

                case tracks::Envelope::kTrackAbort: {
                    player.stop();
                    status.print_aborted(env.track_abort().reason());
                    state = State::STOPPED;
                    break;
                }

                default:
                    // Ignore all MIR analysis events
                    break;
            }
        }

        // Update progress display while playing
        if (state == State::PLAYING) {
            status.update_playing(current_file,
                                  player.get_position_seconds(),
                                  current_duration);

            if (player.is_finished()) {
                status.print_ended();
                state = State::STOPPED;
            }
        }
    }

    // Clean shutdown
    if (g_shutdown.load(std::memory_order_relaxed)) {
        player.stop();
        status.print_shutdown();
    }

    receiver.close();
    google::protobuf::ShutdownProtobufLibrary();
    return 0;
}
