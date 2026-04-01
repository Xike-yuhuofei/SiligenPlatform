#include "runtime/dispensing/WorkflowDispensingProcessPortAdapter.h"

#include "modules/dispense-packaging/domain/dispensing/domain-services/DispensingProcessService.h"

#include <utility>

namespace Siligen::Runtime::Service::Dispensing {

using Siligen::Domain::Dispensing::DomainServices::DispensingProcessService;
using Siligen::Domain::Dispensing::Ports::IDispensingExecutionObserver;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionOptions;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionPlan;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionReport;
using Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeOverrides;
using Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeParams;
using Siligen::Shared::Types::Result;

WorkflowDispensingProcessPortAdapter::WorkflowDispensingProcessPortAdapter(
    std::shared_ptr<Siligen::Domain::Dispensing::Ports::IValvePort> valve_port,
    std::shared_ptr<Siligen::Domain::Motion::Ports::IInterpolationPort> interpolation_port,
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionStatePort> motion_state_port,
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port)
    : service_(std::make_shared<DispensingProcessService>(
          std::move(valve_port),
          std::move(interpolation_port),
          std::move(motion_state_port),
          std::move(connection_port),
          std::move(config_port))) {}

Result<void> WorkflowDispensingProcessPortAdapter::ValidateHardwareConnection() noexcept {
    return service_->ValidateHardwareConnection();
}

Result<DispensingRuntimeParams> WorkflowDispensingProcessPortAdapter::BuildRuntimeParams(
    const DispensingRuntimeOverrides& overrides) noexcept {
    return service_->BuildRuntimeParams(overrides);
}

Result<DispensingExecutionReport> WorkflowDispensingProcessPortAdapter::ExecuteProcess(
    const DispensingExecutionPlan& plan,
    const DispensingRuntimeParams& params,
    const DispensingExecutionOptions& options,
    std::atomic<bool>* stop_flag,
    std::atomic<bool>* pause_flag,
    std::atomic<bool>* pause_applied_flag,
    IDispensingExecutionObserver* observer) noexcept {
    return service_->ExecuteProcess(plan, params, options, stop_flag, pause_flag, pause_applied_flag, observer);
}

void WorkflowDispensingProcessPortAdapter::StopExecution(
    std::atomic<bool>* stop_flag,
    std::atomic<bool>* pause_flag) noexcept {
    service_->StopExecution(stop_flag, pause_flag);
}

}  // namespace Siligen::Runtime::Service::Dispensing
