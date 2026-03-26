#pragma once

#include "siligen/shared/result_types.h"

#include <string>

namespace Siligen::Process::Ports {

class DispenserActuatorPort {
   public:
    virtual ~DispenserActuatorPort() = default;

    virtual Siligen::SharedKernel::VoidResult Prime(const std::string& execution_id) = 0;
    virtual Siligen::SharedKernel::VoidResult Start(const std::string& execution_id) = 0;
    virtual Siligen::SharedKernel::VoidResult Pause(const std::string& execution_id) = 0;
    virtual Siligen::SharedKernel::VoidResult Resume(const std::string& execution_id) = 0;
    virtual Siligen::SharedKernel::VoidResult Stop(const std::string& execution_id) = 0;
};

}  // namespace Siligen::Process::Ports
