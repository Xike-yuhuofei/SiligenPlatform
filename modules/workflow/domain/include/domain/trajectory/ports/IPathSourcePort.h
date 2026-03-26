#pragma once

#include "domain/trajectory/value-objects/Primitive.h"
#include "shared/types/Result.h"
#include "shared/types/VisualizationTypes.h"

#include <string>
#include <vector>

namespace Siligen::Domain::Trajectory::Ports {

using Siligen::Domain::Trajectory::ValueObjects::Primitive;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::DXFEntityType;
using Siligen::Shared::Types::int32;

struct PathPrimitiveMeta {
    int32 entity_id = -1;
    DXFEntityType entity_type = DXFEntityType::Unknown;
    int32 entity_segment_index = 0;
    bool entity_closed = false;
};

struct PathSourceResult {
    bool success = false;
    std::string error_message;
    std::vector<Primitive> primitives;
    std::vector<PathPrimitiveMeta> metadata;
};

class IPathSourcePort {
   public:
    virtual ~IPathSourcePort() = default;

    virtual Result<PathSourceResult> LoadFromFile(const std::string& filepath) = 0;
};

}  // namespace Siligen::Domain::Trajectory::Ports
