#pragma once

#include <string>
#include <vector>

namespace Siligen::Gateway::Cli {

struct CliCommandRequest {
    std::string raw_command;
    std::vector<std::string> arguments;
    bool interactive = false;
};

}  // namespace Siligen::Gateway::Cli
