#include "udp_receiver.h"
#include <cstdio>
#include <stdexcept>

UdpReceiver::UdpReceiver(uint16_t port) {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        throw std::runtime_error("WSAStartup failed");
    }
    wsa_initialized_ = true;

    sock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_ == INVALID_SOCKET) {
        throw std::runtime_error("Failed to create UDP socket");
    }

    // Allow address reuse
    int reuse = 1;
    setsockopt(sock_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    // Set receive timeout to 500ms so the main loop can check shutdown flag
    DWORD timeout_ms = 500;
    setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO,
               reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
        throw std::runtime_error("Failed to bind UDP socket");
    }
}

UdpReceiver::~UdpReceiver() {
    close();
}

int UdpReceiver::receive(char* buf, int buf_size) {
    if (sock_ == INVALID_SOCKET) return -1;

    sockaddr_in sender{};
    int sender_len = sizeof(sender);
    int len = recvfrom(sock_, buf, buf_size, 0,
                       reinterpret_cast<sockaddr*>(&sender), &sender_len);

    if (len == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err == WSAETIMEDOUT) return 0;  // timeout — not an error
        return -1;
    }
    return len;
}

void UdpReceiver::close() {
    if (sock_ != INVALID_SOCKET) {
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }
    if (wsa_initialized_) {
        WSACleanup();
        wsa_initialized_ = false;
    }
}
