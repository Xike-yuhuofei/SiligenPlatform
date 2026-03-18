#pragma once

#include "siligen/process/execution/process_types.h"
#include "siligen/shared/result_types.h"

namespace Siligen::Process::Services {

class ProcessExecutionService {
   public:
    virtual ~ProcessExecutionService() = default;

    virtual Siligen::SharedKernel::Result<Siligen::Process::Execution::ProcessExecutionSummary> Plan(
        const Siligen::Process::Execution::ProcessExecutionRequest& request) = 0;
    virtual Siligen::SharedKernel::VoidResult Execute(
        const Siligen::Process::Execution::ProcessExecutionRequest& request) = 0;
};

}  // namespace Siligen::Process::Services
