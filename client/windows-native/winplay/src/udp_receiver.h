#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#include <vector>

class UdpReceiver {
public:
    explicit UdpReceiver(uint16_t port);
    ~UdpReceiver();

    UdpReceiver(const UdpReceiver&) = delete;
    UdpReceiver& operator=(const UdpReceiver&) = delete;

    // Returns number of bytes received, 0 on timeout, -1 on error.
    int receive(char* buf, int buf_size);

    void close();

private:
    SOCKET sock_ = INVALID_SOCKET;
    bool wsa_initialized_ = false;
};
