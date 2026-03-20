#include "shared/Strings/StringValidator.h"

#include "siligen/shared/strings/string_manipulator.h"

#include <boost/algorithm/string.hpp>
#include <boost/range/iterator_range.hpp>

#include <iterator>

namespace Siligen {

namespace {
using StringManipulator = Siligen::SharedKernel::StringManipulator;
}

bool StringValidator::IsEmpty(const std::string& str) {
    return str.empty();
}

bool StringValidator::IsWhitespace(const std::string& str) {
    return StringManipulator::Trim(str).empty();
}

bool StringValidator::StartsWith(const std::string& str, const std::string& prefix, bool ignore_case) {
    return ignore_case ? boost::algorithm::istarts_with(str, prefix)
                       : boost::algorithm::starts_with(str, prefix);
}

bool StringValidator::EndsWith(const std::string& str, const std::string& suffix, bool ignore_case) {
    return ignore_case ? boost::algorithm::iends_with(str, suffix)
                       : boost::algorithm::ends_with(str, suffix);
}

bool StringValidator::Contains(const std::string& str, const std::string& substring, bool ignore_case) {
    return ignore_case ? boost::algorithm::icontains(str, substring)
                       : boost::algorithm::contains(str, substring);
}

int32 StringValidator::Find(const std::string& str, const std::string& substring, int32 start_pos, bool ignore_case) {
    if (start_pos < 0) start_pos = 0;
    if (substring.empty()) return start_pos;

    auto range = boost::make_iterator_range(str.begin() + start_pos, str.end());
    auto found = ignore_case ? boost::algorithm::ifind_first(range, substring)
                             : boost::algorithm::find_first(range, substring);
    if (found.begin() == found.end()) {
        return -1;
    }
    return static_cast<int32>(std::distance(str.begin(), found.begin()));
}

bool StringValidator::InternalEquals(const std::string& str1, const std::string& str2, bool ignore_case) {
    if (ignore_case) {
        return boost::algorithm::iequals(str1, str2);
    }
    return str1 == str2;
}

}  // namespace Siligen
