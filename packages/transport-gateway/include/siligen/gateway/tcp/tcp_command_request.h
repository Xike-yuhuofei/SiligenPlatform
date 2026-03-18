#pragma once

#include <string>

namespace Siligen::Gateway::Tcp {

struct TcpCommandRequest {
    std::string connection_id;
    std::string remote_endpoint;
    std::string message;
};

}  // namespace Siligen::Gateway::Tcp
