#pragma once

#include <cstdint>

namespace Siligen::Workflow::Domain {

enum class WorkflowArchiveStatus : std::uint8_t {
    None = 0,
    Requested,
    Archived,
    Failed,
};

}  // namespace Siligen::Workflow::Domain
