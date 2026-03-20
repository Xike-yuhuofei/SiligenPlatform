#pragma once

#include "shared/types/Types.h"

#include <string>

namespace Siligen {

class TypeConverter {
   public:
    static int32 StringToInt(const std::string& str, int32 default_value = 0);
    static float32 StringToFloat(const std::string& str, float32 default_value = 0.0f);
    static bool StringToBool(const std::string& str, bool default_value = false);

    static std::string IntToString(int32 value);
    static std::string FloatToString(float32 value, int32 precision = 6);
    static std::string BoolToString(bool value);
};

}  // namespace Siligen

