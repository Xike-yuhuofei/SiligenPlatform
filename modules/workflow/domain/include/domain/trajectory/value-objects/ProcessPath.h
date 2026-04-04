#pragma once

#include "domain/trajectory/value-objects/Primitive.h"
#include "domain/trajectory/value-objects/Path.h"
#include "domain/trajectory/value-objects/GeometryUtils.h"
#include "process_path/contracts/ProcessPath.h"

// Legacy bridge header; canonical public owner contract lives in M6 process-path contracts.
namespace Siligen::Domain::Trajectory::ValueObjects {

using SegmentType = Siligen::ProcessPath::Contracts::SegmentType;
using Segment = Siligen::ProcessPath::Contracts::Segment;
using ProcessTag = Siligen::ProcessPath::Contracts::ProcessTag;
using ProcessConstraint = Siligen::ProcessPath::Contracts::ProcessConstraint;
using ProcessSegment = Siligen::ProcessPath::Contracts::ProcessSegment;
using ProcessPath = Siligen::ProcessPath::Contracts::ProcessPath;

}  // namespace Siligen::Domain::Trajectory::ValueObjects
