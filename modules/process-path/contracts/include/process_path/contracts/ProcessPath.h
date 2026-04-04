#pragma once

#include "process_path/contracts/Path.h"
#include "process_path/contracts/ProcessConfig.h"

#include <vector>

namespace Siligen::ProcessPath::Contracts {

using Siligen::Shared::Types::float32;

enum class ProcessTag {
    Normal,
    Start,
    End,
    Corner,
    Rapid
};

struct ProcessConstraint {
    bool has_velocity_factor = false;
    float32 max_velocity_factor = 1.0f;
    bool has_local_speed_hint = false;
    float32 local_speed_factor = 1.0f;
};

struct ProcessSegment {
    Segment geometry{};
    bool dispense_on = true;
    float32 flow_rate = 0.0f;
    ProcessConstraint constraint{};
    ProcessTag tag = ProcessTag::Normal;
};

struct ProcessPath {
    std::vector<ProcessSegment> segments;
};

}  // namespace Siligen::ProcessPath::Contracts
