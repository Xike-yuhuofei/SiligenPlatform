#include "shared/Strings/StringFormatter.h"

#include "shared/Conversion/TypeConverter.h"

#include <iomanip>
#include <sstream>

namespace Siligen {

std::string StringFormatter::FormatNumber(float32 value, int32 decimal_places) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(decimal_places) << value;
    return oss.str();
}

std::string StringFormatter::FormatScientific(float32 value, int32 precision) {
    std::ostringstream oss;
    oss << std::scientific << std::setprecision(precision) << value;
    return oss.str();
}

std::string StringFormatter::FormatFileSize(int64 bytes) {
    if (bytes < 1024) {
        return TypeConverter::IntToString(static_cast<int32>(bytes)) + " B";
    } else if (bytes < 1024 * 1024) {
        return FormatNumber(static_cast<float32>(bytes) / 1024.0f, 1) + " KB";
    } else if (bytes < 1024 * 1024 * 1024) {
        return FormatNumber(static_cast<float32>(bytes) / (1024.0f * 1024.0f), 1) + " MB";
    } else {
        return FormatNumber(static_cast<float32>(bytes) / (1024.0f * 1024.0f * 1024.0f), 1) + " GB";
    }
}

std::string StringFormatter::FormatDuration(float64 seconds) {
    int32 hours = static_cast<int32>(seconds) / 3600;
    int32 minutes = (static_cast<int32>(seconds) % 3600) / 60;
    int32 secs = static_cast<int32>(seconds) % 60;

    std::ostringstream oss;
    if (hours > 0) {
        oss << std::setfill('0') << std::setw(2) << hours << ":";
    }
    oss << std::setfill('0') << std::setw(2) << minutes << ":";
    oss << std::setfill('0') << std::setw(2) << secs;

    return oss.str();
}

}  // namespace Siligen
