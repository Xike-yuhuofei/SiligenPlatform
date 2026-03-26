#pragma once

#include <map>
#include <string>

namespace Siligen::Gateway::Protocol {

struct RequestEnvelope {
    std::string request_id;
    std::string command;
    std::map<std::string, std::string> metadata;
    std::string payload;
};

}  // namespace Siligen::Gateway::Protocol
