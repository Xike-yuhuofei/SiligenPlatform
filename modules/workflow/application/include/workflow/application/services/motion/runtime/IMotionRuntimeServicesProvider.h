#pragma once

#include "domain/motion/domain-services/MotionControlService.h"
#include "domain/motion/domain-services/MotionStatusService.h"
#include "domain/motion/domain-services/MotionValidationService.h"
#include "domain/motion/ports/IMotionRuntimePort.h"

#include <memory>

namespace Siligen::Application::Services::Motion::Runtime {

struct MotionRuntimeServicesBundle {
    std::shared_ptr<Domain::Motion::DomainServices::MotionControlService> motion_control_service{};
    std::shared_ptr<Domain::Motion::DomainServices::MotionStatusService> motion_status_service{};
    std::shared_ptr<Domain::Motion::DomainServices::MotionValidationService> motion_validation_service{};
};

class IMotionRuntimeServicesProvider {
public:
    virtual ~IMotionRuntimeServicesProvider() = default;

    virtual MotionRuntimeServicesBundle CreateServices(
        const std::shared_ptr<Domain::Motion::Ports::IMotionRuntimePort>& motion_runtime_port) const = 0;
};

}  // namespace Siligen::Application::Services::Motion::Runtime
