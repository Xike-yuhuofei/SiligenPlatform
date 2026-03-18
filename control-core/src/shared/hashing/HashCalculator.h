#pragma once

#include "shared/types/Types.h"

#include <string>

namespace Siligen {

class HashCalculator {
   public:
    static uint32 GetHashCode(const std::string& str);
    static uint32 CalculateCRC32(const uint8* data, int32 length);
    static uint32 CalculateCRC32(const std::string& str);
};

}  // namespace Siligen


