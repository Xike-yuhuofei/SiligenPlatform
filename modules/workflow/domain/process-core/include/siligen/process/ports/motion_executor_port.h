#pragma once

#include "siligen/shared/result_types.h"

#include <string>

namespace Siligen::Process::Ports {

class MotionExecutorPort {
   public:
    virtual ~MotionExecutorPort() = default;

    virtual Siligen::SharedKernel::VoidResult PrepareExecution(const std::string& execution_id) = 0;
    virtual Siligen::SharedKernel::VoidResult ExecutePlan(const std::string& execution_id) = 0;
    virtual Siligen::SharedKernel::VoidResult StopExecution(const std::string& execution_id) = 0;
};

}  // namespace Siligen::Process::Ports
