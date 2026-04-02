#pragma once

#include "domain/configuration/ports/IConfigurationPort.h"
#include "runtime/dispensing/WorkflowDispensingProcessOperations.h"
#include "runtime_execution/contracts/dispensing/IDispensingProcessPort.h"
#include "runtime_execution/contracts/dispensing/IValvePort.h"
#include "runtime_execution/contracts/motion/IInterpolationPort.h"
#include "runtime_execution/contracts/motion/IMotionStatePort.h"
#include "siligen/device/contracts/ports/device_ports.h"

#include <memory>

namespace Siligen::Runtime::Service::Dispensing {

class WorkflowDispensingProcessPortAdapter final
    : public Siligen::RuntimeExecution::Contracts::Dispensing::IDispensingProcessPort {
   public:
    ~WorkflowDispensingProcessPortAdapter() override;

    WorkflowDispensingProcessPortAdapter(
        std::shared_ptr<Siligen::Domain::Dispensing::Ports::IValvePort> valve_port,
        std::shared_ptr<Siligen::Domain::Motion::Ports::IInterpolationPort> interpolation_port,
        std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionStatePort> motion_state_port,
        std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
        std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port);

#ifdef SILIGEN_TEST_HOOKS
    explicit WorkflowDispensingProcessPortAdapter(
        std::shared_ptr<IWorkflowDispensingProcessOperations> process_operations);
#endif

    Siligen::Shared::Types::Result<void> ValidateHardwareConnection() noexcept override;
    Siligen::Shared::Types::Result<Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeParams>
    BuildRuntimeParams(
        const Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeOverrides& overrides) noexcept override;
    Siligen::Shared::Types::Result<Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionReport>
    ExecuteProcess(
        const Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionPlan& plan,
        const Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeParams& params,
        const Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionOptions& options,
        std::atomic<bool>* stop_flag,
        std::atomic<bool>* pause_flag,
        std::atomic<bool>* pause_applied_flag,
        Siligen::Domain::Dispensing::Ports::IDispensingExecutionObserver* observer = nullptr) noexcept override;
    void StopExecution(std::atomic<bool>* stop_flag, std::atomic<bool>* pause_flag = nullptr) noexcept override;

   private:
    std::shared_ptr<IWorkflowDispensingProcessOperations> process_operations_;
};

}  // namespace Siligen::Runtime::Service::Dispensing
