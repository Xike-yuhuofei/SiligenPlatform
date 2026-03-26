#include "shared/Strings/StringManipulator.h"

#include "siligen/shared/strings/string_manipulator.h"

namespace Siligen {

std::string StringManipulator::Trim(const std::string& str) {
    return Siligen::SharedKernel::StringManipulator::Trim(str);
}

std::string StringManipulator::TrimLeft(const std::string& str) {
    return Siligen::SharedKernel::StringManipulator::TrimLeft(str);
}

std::string StringManipulator::TrimRight(const std::string& str) {
    return Siligen::SharedKernel::StringManipulator::TrimRight(str);
}

std::string StringManipulator::StripInlineComment(const std::string& str) {
    return Siligen::SharedKernel::StringManipulator::StripInlineComment(str);
}

std::string StringManipulator::ToLower(const std::string& str) {
    return Siligen::SharedKernel::StringManipulator::ToLower(str);
}

std::string StringManipulator::ToUpper(const std::string& str) {
    return Siligen::SharedKernel::StringManipulator::ToUpper(str);
}

std::string StringManipulator::Replace(const std::string& str, const std::string& from, const std::string& to) {
    return Siligen::SharedKernel::StringManipulator::Replace(str, from, to);
}

std::string StringManipulator::ReplaceAll(const std::string& str, const std::string& from, const std::string& to) {
    return Siligen::SharedKernel::StringManipulator::ReplaceAll(str, from, to);
}

std::vector<std::string> StringManipulator::Split(const std::string& str,
                                                  const std::string& delimiter,
                                                  int32 max_split) {
    return Siligen::SharedKernel::StringManipulator::Split(str, delimiter, max_split);
}

std::vector<std::string> StringManipulator::SplitLines(const std::string& str) {
    return Siligen::SharedKernel::StringManipulator::SplitLines(str);
}

std::string StringManipulator::Join(const std::vector<std::string>& strings, const std::string& separator) {
    return Siligen::SharedKernel::StringManipulator::Join(strings, separator);
}

std::string StringManipulator::PadLeft(const std::string& str, int32 width, char fill_char) {
    return Siligen::SharedKernel::StringManipulator::PadLeft(str, width, fill_char);
}

std::string StringManipulator::PadRight(const std::string& str, int32 width, char fill_char) {
    return Siligen::SharedKernel::StringManipulator::PadRight(str, width, fill_char);
}

std::string StringManipulator::InternalTrim(const std::string& str, bool left, bool right) {
    if (left && right) {
        return Siligen::SharedKernel::StringManipulator::Trim(str);
    }
    if (left) {
        return Siligen::SharedKernel::StringManipulator::TrimLeft(str);
    }
    if (right) {
        return Siligen::SharedKernel::StringManipulator::TrimRight(str);
    }
    return str;
}

std::vector<std::string> StringManipulator::InternalSplit(const std::string& str,
                                                          const std::string& delimiter,
                                                          int32 max_split) {
    return Siligen::SharedKernel::StringManipulator::Split(str, delimiter, max_split);
}

}  // namespace Siligen
