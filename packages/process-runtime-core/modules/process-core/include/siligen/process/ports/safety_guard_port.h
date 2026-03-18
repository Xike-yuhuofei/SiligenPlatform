#pragma once

#include "siligen/shared/result_types.h"

#include <string>

namespace Siligen::Process::Ports {

class SafetyGuardPort {
   public:
    virtual ~SafetyGuardPort() = default;

    virtual Siligen::SharedKernel::Result<bool> CanStartExecution(const std::string& execution_id) const = 0;
    virtual Siligen::SharedKernel::VoidResult ValidateExecution(const std::string& execution_id) const = 0;
};

}  // namespace Siligen::Process::Ports
