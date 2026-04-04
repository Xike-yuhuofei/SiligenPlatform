#pragma once

#include "domain/trajectory/value-objects/Primitive.h"
#include "domain/trajectory/value-objects/ProcessPath.h"
#include "process_path/contracts/GeometryUtils.h"

// Legacy bridge header; canonical public owner contract lives in M6 process-path contracts.
namespace Siligen::Domain::Trajectory::ValueObjects {

using Siligen::ProcessPath::Contracts::kGeometryEpsilon;
using Siligen::ProcessPath::Contracts::kEllipseEpsilon;
using Siligen::ProcessPath::Contracts::Clamp;
using Siligen::ProcessPath::Contracts::NormalizeAngle;
using Siligen::ProcessPath::Contracts::NormalizeSweep;
using Siligen::ProcessPath::Contracts::ComputeArcSweep;
using Siligen::ProcessPath::Contracts::ComputeArcLength;
using Siligen::ProcessPath::Contracts::ArcPoint;
using Siligen::ProcessPath::Contracts::ArcTangent;
using Siligen::ProcessPath::Contracts::LineDirection;
using Siligen::ProcessPath::Contracts::SegmentStart;
using Siligen::ProcessPath::Contracts::SegmentEnd;
using Siligen::ProcessPath::Contracts::EllipsePoint;

}  // namespace Siligen::Domain::Trajectory::ValueObjects
