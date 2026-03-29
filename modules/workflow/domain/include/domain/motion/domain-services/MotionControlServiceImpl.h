#pragma once

// Public compatibility shim. Runtime concrete owner lives in runtime-execution.
#include "runtime_execution/application/services/motion/MotionControlServiceImpl.h"

namespace Siligen::Domain::Motion::DomainServices {

using MotionControlServiceImpl = Siligen::RuntimeExecution::Application::Services::Motion::MotionControlServiceImpl;

}  // namespace Siligen::Domain::Motion::DomainServices
