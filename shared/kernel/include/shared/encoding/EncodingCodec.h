#pragma once

#include "shared/types/Types.h"

#include <string>
#include <vector>

namespace Siligen {

class EncodingCodec {
   public:
    static std::string UrlEncode(const std::string& str);
    static std::string UrlDecode(const std::string& str);

    static std::string Base64Encode(const uint8* data, int32 length);
    static std::vector<uint8> Base64Decode(const std::string& str);
};

}  // namespace Siligen


