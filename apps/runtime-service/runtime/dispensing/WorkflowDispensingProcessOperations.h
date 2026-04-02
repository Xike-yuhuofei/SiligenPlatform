#pragma once

#include "domain/configuration/ports/IConfigurationPort.h"
#include "domain/dispensing/ports/IDispensingExecutionObserver.h"
#include "domain/dispensing/ports/IValvePort.h"
#include "domain/dispensing/value-objects/DispensingExecutionTypes.h"
#include "domain/motion/ports/IInterpolationPort.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "shared/types/Result.h"
#include "siligen/device/contracts/ports/device_ports.h"

#include <atomic>
#include <memory>

namespace Siligen::Runtime::Service::Dispensing {

class IWorkflowDispensingProcessOperations {
   public:
    virtual ~IWorkflowDispensingProcessOperations() = default;

    virtual Siligen::Shared::Types::Result<void> ValidateHardwareConnection() noexcept = 0;
    virtual Siligen::Shared::Types::Result<Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeParams>
    BuildRuntimeParams(
        const Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeOverrides& overrides) noexcept = 0;
    virtual Siligen::Shared::Types::Result<Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionReport>
    ExecuteProcess(
        const Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionPlan& plan,
        const Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeParams& params,
        const Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionOptions& options,
        std::atomic<bool>* stop_flag,
        std::atomic<bool>* pause_flag,
        std::atomic<bool>* pause_applied_flag,
        Siligen::Domain::Dispensing::Ports::IDispensingExecutionObserver* observer = nullptr) noexcept = 0;
    virtual void StopExecution(
        std::atomic<bool>* stop_flag,
        std::atomic<bool>* pause_flag = nullptr) noexcept = 0;
};

std::shared_ptr<IWorkflowDispensingProcessOperations> CreateWorkflowDispensingProcessOperations(
    std::shared_ptr<Siligen::Domain::Dispensing::Ports::IValvePort> valve_port,
    std::shared_ptr<Siligen::Domain::Motion::Ports::IInterpolationPort> interpolation_port,
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionStatePort> motion_state_port,
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port);

}  // namespace Siligen::Runtime::Service::Dispensing
