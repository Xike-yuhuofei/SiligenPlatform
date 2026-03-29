#pragma once

#include "../../../../domain/trajectory/value-objects/GeometryUtils.h"

namespace Siligen::ProcessPath::Contracts {

using Siligen::Domain::Trajectory::ValueObjects::ArcPoint;
using Siligen::Domain::Trajectory::ValueObjects::ArcTangent;
using Siligen::Domain::Trajectory::ValueObjects::Clamp;
using Siligen::Domain::Trajectory::ValueObjects::ComputeArcLength;
using Siligen::Domain::Trajectory::ValueObjects::ComputeArcSweep;
using Siligen::Domain::Trajectory::ValueObjects::EllipsePoint;
using Siligen::Domain::Trajectory::ValueObjects::LineDirection;
using Siligen::Domain::Trajectory::ValueObjects::NormalizeAngle;
using Siligen::Domain::Trajectory::ValueObjects::NormalizeSweep;
using Siligen::Domain::Trajectory::ValueObjects::SegmentEnd;
using Siligen::Domain::Trajectory::ValueObjects::SegmentStart;

}  // namespace Siligen::ProcessPath::Contracts
