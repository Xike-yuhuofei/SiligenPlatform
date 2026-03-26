#pragma once

#include "shared/types/Types.h"

#include <spdlog/fmt/bundled/printf.h>

#include <string>
#include <vector>

namespace Siligen {

class StringFormatter {
   public:
    template <typename... Args>
    static std::string Format(const std::string& format, Args... args);

    static std::string FormatNumber(float32 value, int32 decimal_places = 2);
    static std::string FormatScientific(float32 value, int32 precision = 6);
    static std::string FormatFileSize(int64 bytes);
    static std::string FormatDuration(float64 seconds);
};

template <typename... Args>
std::string StringFormatter::Format(const std::string& format, Args... args) {
    try {
        return fmt::sprintf(format, args...);
    } catch (...) {
        return "";
    }
}

}  // namespace Siligen

