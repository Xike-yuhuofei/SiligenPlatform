#include "runtime/dispensing/DispensingProcessPortAdapter.h"

#include "modules/dispense-packaging/domain/dispensing/domain-services/DispensingProcessService.h"

#include <utility>

namespace Siligen::RuntimeExecution::Host::Dispensing {
namespace {

using Siligen::Domain::Dispensing::DomainServices::DispensingProcessService;
using Siligen::Domain::Dispensing::Ports::IDispensingExecutionObserver;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionOptions;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionPlan;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionReport;
using Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeOverrides;
using Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeParams;
using Siligen::Shared::Types::Result;

class DispensingProcessOperations final : public IDispensingProcessOperations {
   public:
    explicit DispensingProcessOperations(std::shared_ptr<DispensingProcessService> process_service)
        : process_service_(std::move(process_service)) {}

    Result<void> ValidateHardwareConnection() noexcept override {
        return process_service_->ValidateHardwareConnection();
    }

    Result<DispensingRuntimeParams> BuildRuntimeParams(
        const DispensingRuntimeOverrides& overrides) noexcept override {
        return process_service_->BuildRuntimeParams(overrides);
    }

    Result<DispensingExecutionReport> ExecuteProcess(
        const DispensingExecutionPlan& plan,
        const DispensingRuntimeParams& params,
        const DispensingExecutionOptions& options,
        std::atomic<bool>* stop_flag,
        std::atomic<bool>* pause_flag,
        std::atomic<bool>* pause_applied_flag,
        IDispensingExecutionObserver* observer) noexcept override {
        return process_service_->ExecuteProcess(
            plan,
            params,
            options,
            stop_flag,
            pause_flag,
            pause_applied_flag,
            observer);
    }

    void StopExecution(std::atomic<bool>* stop_flag, std::atomic<bool>* pause_flag) noexcept override {
        process_service_->StopExecution(stop_flag, pause_flag);
    }

   private:
    std::shared_ptr<DispensingProcessService> process_service_;
};

}  // namespace

std::shared_ptr<IDispensingProcessOperations> CreateDispensingProcessOperations(
    std::shared_ptr<Siligen::Domain::Dispensing::Ports::IValvePort> valve_port,
    std::shared_ptr<Siligen::Domain::Motion::Ports::IInterpolationPort> interpolation_port,
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionStatePort> motion_state_port,
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port) {
    return std::make_shared<DispensingProcessOperations>(std::make_shared<DispensingProcessService>(
        std::move(valve_port),
        std::move(interpolation_port),
        std::move(motion_state_port),
        std::move(connection_port),
        std::move(config_port)));
}

DispensingProcessPortAdapter::~DispensingProcessPortAdapter() = default;

DispensingProcessPortAdapter::DispensingProcessPortAdapter(
    std::shared_ptr<Siligen::Domain::Dispensing::Ports::IValvePort> valve_port,
    std::shared_ptr<Siligen::Domain::Motion::Ports::IInterpolationPort> interpolation_port,
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionStatePort> motion_state_port,
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port)
    : process_operations_(CreateDispensingProcessOperations(
          std::move(valve_port),
          std::move(interpolation_port),
          std::move(motion_state_port),
          std::move(connection_port),
          std::move(config_port))) {}

DispensingProcessPortAdapter::DispensingProcessPortAdapter(
    std::shared_ptr<IDispensingProcessOperations> process_operations)
    : process_operations_(std::move(process_operations)) {}

Result<void> DispensingProcessPortAdapter::ValidateHardwareConnection() noexcept {
    return process_operations_->ValidateHardwareConnection();
}

Result<DispensingRuntimeParams> DispensingProcessPortAdapter::BuildRuntimeParams(
    const DispensingRuntimeOverrides& overrides) noexcept {
    return process_operations_->BuildRuntimeParams(overrides);
}

Result<DispensingExecutionReport> DispensingProcessPortAdapter::ExecuteProcess(
    const DispensingExecutionPlan& plan,
    const DispensingRuntimeParams& params,
    const DispensingExecutionOptions& options,
    std::atomic<bool>* stop_flag,
    std::atomic<bool>* pause_flag,
    std::atomic<bool>* pause_applied_flag,
    IDispensingExecutionObserver* observer) noexcept {
    return process_operations_->ExecuteProcess(
        plan,
        params,
        options,
        stop_flag,
        pause_flag,
        pause_applied_flag,
        observer);
}

void DispensingProcessPortAdapter::StopExecution(
    std::atomic<bool>* stop_flag,
    std::atomic<bool>* pause_flag) noexcept {
    process_operations_->StopExecution(stop_flag, pause_flag);
}

std::shared_ptr<Siligen::RuntimeExecution::Contracts::Dispensing::IDispensingProcessPort> CreateDispensingProcessPort(
    std::shared_ptr<Siligen::Domain::Dispensing::Ports::IValvePort> valve_port,
    std::shared_ptr<Siligen::Domain::Motion::Ports::IInterpolationPort> interpolation_port,
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionStatePort> motion_state_port,
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port) {
    return std::make_shared<DispensingProcessPortAdapter>(
        std::move(valve_port),
        std::move(interpolation_port),
        std::move(motion_state_port),
        std::move(connection_port),
        std::move(config_port));
}

}  // namespace Siligen::RuntimeExecution::Host::Dispensing
