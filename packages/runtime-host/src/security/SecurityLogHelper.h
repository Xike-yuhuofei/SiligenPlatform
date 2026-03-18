#pragma once

#include "shared/interfaces/ILoggingService.h"

#include <string>

namespace Siligen::SecurityLogHelper {

inline void Log(Shared::Types::LogLevel level, const std::string& category, const std::string& message) {
    auto logger = Shared::Interfaces::ILoggingService::GetInstance();
    if (logger) {
        logger->Log(level, message, category);
    }
}

}  // namespace Siligen::SecurityLogHelper
