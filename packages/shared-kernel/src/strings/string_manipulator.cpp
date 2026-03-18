#include "siligen/shared/strings/string_manipulator.h"

#include <boost/algorithm/string.hpp>

#include <cctype>
#include <sstream>

namespace Siligen::SharedKernel {

std::string StringManipulator::Trim(const std::string& str) {
    return boost::algorithm::trim_copy(str);
}

std::string StringManipulator::TrimLeft(const std::string& str) {
    return boost::algorithm::trim_left_copy(str);
}

std::string StringManipulator::TrimRight(const std::string& str) {
    return boost::algorithm::trim_right_copy(str);
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
    return boost::algorithm::to_lower_copy(str);
}

std::string StringManipulator::ToUpper(const std::string& str) {
    return boost::algorithm::to_upper_copy(str);
}

std::string StringManipulator::Replace(const std::string& str, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return str;
    }
    return boost::algorithm::replace_first_copy(str, from, to);
}

std::string StringManipulator::ReplaceAll(const std::string& str, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return str;
    }
    return boost::algorithm::replace_all_copy(str, from, to);
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
    return boost::algorithm::join(strings, separator);
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
