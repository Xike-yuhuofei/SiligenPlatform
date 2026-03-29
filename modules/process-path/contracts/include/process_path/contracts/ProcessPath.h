#pragma once

#include "../../../../domain/trajectory/value-objects/ProcessPath.h"
#include "process_path/contracts/Path.h"
#include "process_path/contracts/Primitive.h"
#include "process_path/contracts/ProcessConfig.h"

namespace Siligen::ProcessPath::Contracts {

using ProcessTag = Siligen::Domain::Trajectory::ValueObjects::ProcessTag;
using ProcessConstraint = Siligen::Domain::Trajectory::ValueObjects::ProcessConstraint;
using ProcessSegment = Siligen::Domain::Trajectory::ValueObjects::ProcessSegment;
using ProcessPath = Siligen::Domain::Trajectory::ValueObjects::ProcessPath;

}  // namespace Siligen::ProcessPath::Contracts
