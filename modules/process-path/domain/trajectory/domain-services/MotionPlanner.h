#pragma once

// Compatibility wrapper. Canonical owner moved to:
// modules/motion-planning/domain/motion/domain-services/MotionPlanner.h
#include "../../../../motion-planning/domain/motion/domain-services/MotionPlanner.h"

namespace Siligen::Domain::Trajectory::DomainServices {

using MotionPlanner = Siligen::Domain::Motion::DomainServices::MotionPlanner;

}  // namespace Siligen::Domain::Trajectory::DomainServices
