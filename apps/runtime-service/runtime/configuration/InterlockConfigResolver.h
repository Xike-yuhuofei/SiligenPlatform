#pragma once

#include "security/InterlockMonitor.h"

#include <string>
#include <vector>

namespace Siligen::Infrastructure::Configuration {

struct InterlockConfigResolution {
    InterlockConfig config{};
    bool explicit_interlock_section = false;
    bool used_compat_fallback = false;
    std::vector<std::string> warnings;
};

InterlockConfigResolution ResolveInterlockConfigFromIni(const std::string& config_path);

}  // namespace Siligen::Infrastructure::Configuration
