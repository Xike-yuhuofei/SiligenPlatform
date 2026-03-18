#pragma once

#include <string>

namespace Siligen::Gateway::Protocol {

struct ResponseEnvelope {
    std::string request_id;
    bool success = false;
    int error_code = 0;
    std::string message;
    std::string payload;
};

}  // namespace Siligen::Gateway::Protocol
