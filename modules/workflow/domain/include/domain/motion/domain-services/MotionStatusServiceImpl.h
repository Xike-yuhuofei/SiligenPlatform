#pragma once

// Public compatibility shim. Runtime concrete owner lives in runtime-execution.
#include "runtime_execution/application/services/motion/MotionStatusServiceImpl.h"

namespace Siligen::Domain::Motion::DomainServices {

using MotionStatusServiceImpl = Siligen::RuntimeExecution::Application::Services::Motion::MotionStatusServiceImpl;

}  // namespace Siligen::Domain::Motion::DomainServices
