#include "shared/hashing/HashCalculator.h"

#include <boost/crc.hpp>

namespace Siligen {

uint32 HashCalculator::GetHashCode(const std::string& str) {
    uint32 hash = 5381;
    for (char c : str) {
        hash = ((hash << 5) + hash) + static_cast<uint8>(c);
    }
    return hash;
}

uint32 HashCalculator::CalculateCRC32(const uint8* data, int32 length) {
    if (!data || length <= 0) {
        return 0;
    }

    boost::crc_32_type crc;
    crc.process_bytes(data, static_cast<std::size_t>(length));
    return static_cast<uint32>(crc.checksum());
}

uint32 HashCalculator::CalculateCRC32(const std::string& str) {
    return CalculateCRC32(reinterpret_cast<const uint8*>(str.c_str()), static_cast<int32>(str.length()));
}

}  // namespace Siligen

