#pragma once

#include "domain/dispensing/ports/ITaskSchedulerPort.h"

namespace Siligen::RuntimeExecution::Contracts::Dispensing {

using ITaskSchedulerPort = Siligen::Domain::Dispensing::Ports::ITaskSchedulerPort;
using TaskExecutor = Siligen::Domain::Dispensing::Ports::TaskExecutor;
using TaskStatus = Siligen::Domain::Dispensing::Ports::TaskStatus;
using TaskStatusInfo = Siligen::Domain::Dispensing::Ports::TaskStatusInfo;

}  // namespace Siligen::RuntimeExecution::Contracts::Dispensing
