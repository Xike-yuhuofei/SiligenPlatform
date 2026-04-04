#pragma once

#include "domain/configuration/ports/IConfigurationPort.h"
#include "domain/dispensing/ports/IDispensingExecutionObserver.h"
#include "domain/dispensing/ports/IValvePort.h"
#include "domain/dispensing/value-objects/DispensingExecutionTypes.h"
#include "domain/motion/ports/IInterpolationPort.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "runtime_execution/contracts/dispensing/IDispensingProcessPort.h"
#include "siligen/device/contracts/ports/device_ports.h"

#include <atomic>
#include <memory>

namespace Siligen::RuntimeExecution::Host::Dispensing {

class IDispensingProcessOperations {
   public:
    virtual ~IDispensingProcessOperations() = default;

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

std::shared_ptr<IDispensingProcessOperations> CreateDispensingProcessOperations(
    std::shared_ptr<Siligen::Domain::Dispensing::Ports::IValvePort> valve_port,
    std::shared_ptr<Siligen::Domain::Motion::Ports::IInterpolationPort> interpolation_port,
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionStatePort> motion_state_port,
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port);

class DispensingProcessPortAdapter final
    : public Siligen::RuntimeExecution::Contracts::Dispensing::IDispensingProcessPort {
   public:
    ~DispensingProcessPortAdapter() override;

    DispensingProcessPortAdapter(
        std::shared_ptr<Siligen::Domain::Dispensing::Ports::IValvePort> valve_port,
        std::shared_ptr<Siligen::Domain::Motion::Ports::IInterpolationPort> interpolation_port,
        std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionStatePort> motion_state_port,
        std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
        std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port);

    explicit DispensingProcessPortAdapter(std::shared_ptr<IDispensingProcessOperations> process_operations);

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
    std::shared_ptr<IDispensingProcessOperations> process_operations_;
};

std::shared_ptr<Siligen::RuntimeExecution::Contracts::Dispensing::IDispensingProcessPort> CreateDispensingProcessPort(
    std::shared_ptr<Siligen::Domain::Dispensing::Ports::IValvePort> valve_port,
    std::shared_ptr<Siligen::Domain::Motion::Ports::IInterpolationPort> interpolation_port,
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionStatePort> motion_state_port,
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port);

}  // namespace Siligen::RuntimeExecution::Host::Dispensing
