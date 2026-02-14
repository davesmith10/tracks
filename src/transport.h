#pragma once

#include "config.h"
#include <string>
#include <boost/asio.hpp>

namespace tracks {

class Transport {
public:
    explicit Transport(const Config& cfg);
    void send(const std::string& serialized_envelope);

private:
    boost::asio::io_context        io_;
    boost::asio::ip::udp::endpoint endpoint_;
    boost::asio::ip::udp::socket   socket_;
};

} // namespace tracks
