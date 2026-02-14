#include "transport.h"
#include <iostream>
#include <array>
#include <cstdio>

namespace tracks {

std::string Transport::detect_wsl2_host() {
    std::array<char, 256> buf;
    std::string result;
    FILE* pipe = popen("ip route show default 2>/dev/null", "r");
    if (!pipe) return {};
    while (fgets(buf.data(), buf.size(), pipe)) {
        result += buf.data();
    }
    pclose(pipe);

    // Parse: "default via 172.x.x.x dev eth0 ..."
    auto pos = result.find("via ");
    if (pos == std::string::npos) return {};
    pos += 4;
    auto end = result.find(' ', pos);
    if (end == std::string::npos) end = result.size();
    return result.substr(pos, end - pos);
}

Transport::Transport(const Config& cfg)
    : endpoint_(boost::asio::ip::address::from_string(cfg.multicast_group), cfg.port)
    , socket_(io_, endpoint_.protocol())
{
    // Set multicast TTL
    socket_.set_option(boost::asio::ip::multicast::hops(cfg.ttl));

    // Enable/disable loopback
    socket_.set_option(boost::asio::ip::multicast::enable_loopback(cfg.loopback));

    // Bind to outbound interface
    if (cfg.interface != "0.0.0.0") {
        socket_.set_option(boost::asio::ip::multicast::outbound_interface(
            boost::asio::ip::address_v4::from_string(cfg.interface)));
    }

    // Unicast dual-send for WSL2
    if (cfg.enable_unicast) {
        std::string target = cfg.unicast_target;
        if (target.empty()) {
            target = detect_wsl2_host();
        }
        if (target.empty()) {
            std::cerr << "Warning: --enable-unicast set but could not detect WSL2 host IP. "
                         "Use --unicast-target to specify manually.\n";
        } else {
            unicast_enabled_ = true;
            unicast_endpoint_ = boost::asio::ip::udp::endpoint(
                boost::asio::ip::address::from_string(target), cfg.port);
            std::cout << "Unicast enabled: also sending to " << target << ":" << cfg.port << std::endl;
        }
    }
}

void Transport::send(const std::string& serialized_envelope) {
    boost::system::error_code ec;
    socket_.send_to(boost::asio::buffer(serialized_envelope), endpoint_, 0, ec);
    if (ec) {
        std::cerr << "send error: " << ec.message() << "\n";
    }

    if (unicast_enabled_) {
        socket_.send_to(boost::asio::buffer(serialized_envelope), unicast_endpoint_, 0, ec);
        if (ec) {
            std::cerr << "unicast send error: " << ec.message() << "\n";
        }
    }
}

} // namespace tracks
