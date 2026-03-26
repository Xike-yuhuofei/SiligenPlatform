#pragma once

#include "shared/types/Types.h"

#include <string>

namespace Siligen {

class StringValidator {
   public:
    static bool IsEmpty(const std::string& str);
    static bool IsWhitespace(const std::string& str);

    static bool StartsWith(const std::string& str, const std::string& prefix, bool ignore_case = false);
    static bool EndsWith(const std::string& str, const std::string& suffix, bool ignore_case = false);
    static bool Contains(const std::string& str, const std::string& substring, bool ignore_case = false);

    static int32 Find(const std::string& str,
                      const std::string& substring,
                      int32 start_pos = 0,
                      bool ignore_case = false);

   private:
    static bool InternalEquals(const std::string& str1, const std::string& str2, bool ignore_case);
};

}  // namespace Siligen

