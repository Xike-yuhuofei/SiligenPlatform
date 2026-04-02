#pragma once

#include "domain/motion/domain-services/MotionControlService.h"
#include "domain/motion/domain-services/MotionStatusService.h"
#include "domain/motion/domain-services/MotionValidationService.h"
#include "runtime_execution/contracts/motion/IMotionRuntimePort.h"

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
        const std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IMotionRuntimePort>& motion_runtime_port)
        const = 0;
};

}  // namespace Siligen::Application::Services::Motion::Runtime
