#include "siligen/shared/strings/string_manipulator.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace Siligen::SharedKernel {

namespace {

bool IsWhitespace(unsigned char value) {
    return std::isspace(value) != 0;
}

std::string TrimLeftCopy(const std::string& str) {
    const auto first = std::find_if_not(str.begin(), str.end(), [](unsigned char value) {
        return IsWhitespace(value);
    });
    return std::string(first, str.end());
}

std::string TrimRightCopy(const std::string& str) {
    const auto last = std::find_if_not(str.rbegin(), str.rend(), [](unsigned char value) {
        return IsWhitespace(value);
    });
    return std::string(str.begin(), last.base());
}

std::string ReplaceOnceCopy(const std::string& str, const std::string& from, const std::string& to) {
    const auto pos = str.find(from);
    if (pos == std::string::npos) {
        return str;
    }

    std::string result;
    result.reserve(str.size() - from.size() + to.size());
    result.append(str, 0, pos);
    result.append(to);
    result.append(str, pos + from.size(), std::string::npos);
    return result;
}

std::string ReplaceAllCopy(const std::string& str, const std::string& from, const std::string& to) {
    std::string result;
    size_t start = 0;
    size_t pos = str.find(from);

    while (pos != std::string::npos) {
        result.append(str, start, pos - start);
        result.append(to);
        start = pos + from.size();
        pos = str.find(from, start);
    }

    result.append(str, start, std::string::npos);
    return result;
}

}  // namespace

std::string StringManipulator::Trim(const std::string& str) {
    return TrimRightCopy(TrimLeftCopy(str));
}

std::string StringManipulator::TrimLeft(const std::string& str) {
    return TrimLeftCopy(str);
}

std::string StringManipulator::TrimRight(const std::string& str) {
    return TrimRightCopy(str);
}

std::string StringManipulator::StripInlineComment(const std::string& str) {
    for (size_t i = 0; i < str.size(); ++i) {
        const char c = str[i];
        if ((c == ';' || c == '#') && (i == 0 || std::isspace(static_cast<unsigned char>(str[i - 1])))) {
            return Trim(str.substr(0, i));
        }
    }
    return Trim(str);
}

std::string StringManipulator::ToLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return result;
}

std::string StringManipulator::ToUpper(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char value) {
        return static_cast<char>(std::toupper(value));
    });
    return result;
}

std::string StringManipulator::Replace(const std::string& str, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return str;
    }
    return ReplaceOnceCopy(str, from, to);
}

std::string StringManipulator::ReplaceAll(const std::string& str, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return str;
    }
    return ReplaceAllCopy(str, from, to);
}

std::vector<std::string> StringManipulator::Split(const std::string& str, const std::string& delimiter, int32 max_split) {
    return InternalSplit(str, delimiter, max_split);
}

std::vector<std::string> StringManipulator::SplitLines(const std::string& str) {
    std::vector<std::string> lines;
    std::istringstream iss(str);
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }
    return lines;
}

std::string StringManipulator::Join(const std::vector<std::string>& strings, const std::string& separator) {
    if (strings.empty()) {
        return {};
    }

    std::ostringstream oss;
    for (size_t index = 0; index < strings.size(); ++index) {
        if (index != 0) {
            oss << separator;
        }
        oss << strings[index];
    }
    return oss.str();
}

std::string StringManipulator::PadLeft(const std::string& str, int32 width, char fill_char) {
    if (str.length() >= static_cast<size_t>(width)) {
        return str;
    }
    return std::string(static_cast<size_t>(width) - str.length(), fill_char) + str;
}

std::string StringManipulator::PadRight(const std::string& str, int32 width, char fill_char) {
    if (str.length() >= static_cast<size_t>(width)) {
        return str;
    }
    return str + std::string(static_cast<size_t>(width) - str.length(), fill_char);
}

std::vector<std::string> StringManipulator::InternalSplit(
    const std::string& str,
    const std::string& delimiter,
    int32 max_split) {
    std::vector<std::string> result;

    if (delimiter.empty()) {
        result.push_back(str);
        return result;
    }

    size_t start = 0;
    size_t pos = str.find(delimiter);
    int32 split_count = 0;

    while (pos != std::string::npos && (max_split == 0 || split_count < max_split - 1)) {
        result.push_back(str.substr(start, pos - start));
        start = pos + delimiter.length();
        pos = str.find(delimiter, start);
        ++split_count;
    }

    result.push_back(str.substr(start));
    return result;
}

}  // namespace Siligen::SharedKernel
