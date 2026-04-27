#pragma once

#include "shared/types/Types.h"

#include <string>
#include <vector>

namespace Siligen {

class StringManipulator {
   public:
    static std::string Trim(const std::string& str);
    static std::string TrimLeft(const std::string& str);
    static std::string TrimRight(const std::string& str);
    static std::string StripInlineComment(const std::string& str);

    static std::string ToLower(const std::string& str);
    static std::string ToUpper(const std::string& str);

    static std::string Replace(const std::string& str, const std::string& from, const std::string& to);
    static std::string ReplaceAll(const std::string& str, const std::string& from, const std::string& to);

    static std::vector<std::string> Split(const std::string& str, const std::string& delimiter, int32 max_split = 0);
    static std::vector<std::string> SplitLines(const std::string& str);
    static std::string Join(const std::vector<std::string>& strings, const std::string& separator);

    static std::string PadLeft(const std::string& str, int32 width, char fill_char = ' ');
    static std::string PadRight(const std::string& str, int32 width, char fill_char = ' ');

};

}  // namespace Siligen

