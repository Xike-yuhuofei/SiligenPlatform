#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Siligen::Shared::Hashing {

std::string ComputeSha256Hex(const std::vector<std::uint8_t>& payload);

}  // namespace Siligen::Shared::Hashing
