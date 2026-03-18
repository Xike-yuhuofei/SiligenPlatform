#include "shared/Conversion/TypeConverter.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace Siligen {

int32 TypeConverter::StringToInt(const std::string& str, int32 default_value) {
    if (str.empty()) return default_value;

    // Manual parsing to avoid exceptions
    try {
        size_t pos = 0;
        int32 result = std::stoi(str, &pos);
        // Ensure entire string was consumed
        if (pos == str.length()) {
            return result;
        }
    } catch (...) {
        // Fall through to default
    }
    return default_value;
}

float32 TypeConverter::StringToFloat(const std::string& str, float32 default_value) {
    if (str.empty()) return default_value;

    // Manual parsing to avoid exceptions
    try {
        size_t pos = 0;
        float32 result = std::stof(str, &pos);
        // Ensure entire string was consumed
        if (pos == str.length()) {
            return result;
        }
    } catch (...) {
        // Fall through to default
    }
    return default_value;
}

bool TypeConverter::StringToBool(const std::string& str, bool default_value) {
    if (str.empty()) return default_value;

    std::string lower_str = str;
    std::transform(
        lower_str.begin(), lower_str.end(), lower_str.begin(), [](unsigned char c) { return std::tolower(c); });

    if (lower_str == "true" || lower_str == "1" || lower_str == "yes" || lower_str == "on") {
        return true;
    } else if (lower_str == "false" || lower_str == "0" || lower_str == "no" || lower_str == "off") {
        return false;
    }

    return default_value;
}

std::string TypeConverter::IntToString(int32 value) {
    return std::to_string(value);
}

std::string TypeConverter::FloatToString(float32 value, int32 precision) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

std::string TypeConverter::BoolToString(bool value) {
    return value ? "true" : "false";
}

} // namespace Siligen
