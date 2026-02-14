#include "transport.h"
#include <iostream>

namespace tracks {

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
}

void Transport::send(const std::string& serialized_envelope) {
    boost::system::error_code ec;
    socket_.send_to(boost::asio::buffer(serialized_envelope), endpoint_, 0, ec);
    if (ec) {
        std::cerr << "send error: " << ec.message() << "\n";
    }
}

} // namespace tracks
