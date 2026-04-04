#pragma once

#include "domain/motion/ports/IInterpolationPort.h"

namespace Siligen::RuntimeExecution::Contracts::Motion {

using InterpolationType = Siligen::Domain::Motion::Ports::InterpolationType;
using CoordinateSystemConfig = Siligen::Domain::Motion::Ports::CoordinateSystemConfig;
using InterpolationData = Siligen::Domain::Motion::Ports::InterpolationData;
using CoordinateSystemState = Siligen::Domain::Motion::Ports::CoordinateSystemState;
using CoordinateSystemStatus = Siligen::Domain::Motion::Ports::CoordinateSystemStatus;
using IInterpolationPort = Siligen::Domain::Motion::Ports::IInterpolationPort;

}  // namespace Siligen::RuntimeExecution::Contracts::Motion
