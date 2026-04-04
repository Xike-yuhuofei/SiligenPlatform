#pragma once

#include "shared/types/Types.h"
#include "shared/types/VisualizationTypes.h"

namespace Siligen::ProcessPath::Contracts {

using Siligen::Shared::Types::DXFEntityType;
using Siligen::Shared::Types::int32;

struct PathPrimitiveMeta {
    int32 entity_id = -1;
    DXFEntityType entity_type = DXFEntityType::Unknown;
    int32 entity_segment_index = 0;
    bool entity_closed = false;
};

}  // namespace Siligen::ProcessPath::Contracts
