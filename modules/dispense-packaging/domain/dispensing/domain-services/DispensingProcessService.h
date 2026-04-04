#pragma once

#include "domain/configuration/ports/IConfigurationPort.h"
#include "domain/dispensing/domain-services/DispensingController.h"
#include "domain/dispensing/ports/IDispensingExecutionObserver.h"
#include "domain/dispensing/ports/IValvePort.h"
#include "domain/dispensing/value-objects/DispensingExecutionTypes.h"
#include "runtime_execution/contracts/motion/IInterpolationPort.h"
#include "domain/motion/ports/IMotionStatePort.h"
#include "siligen/device/contracts/ports/device_ports.h"
#include "shared/types/Result.h"

#include <atomic>
#include <memory>

namespace Siligen::Domain::Dispensing::DomainServices {

using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::int32;
using Siligen::Domain::Configuration::Ports::IConfigurationPort;
using Siligen::Domain::Dispensing::Ports::IDispensingExecutionObserver;
using Siligen::Domain::Dispensing::Ports::IValvePort;
using Siligen::RuntimeExecution::Contracts::Motion::IInterpolationPort;
using Siligen::Domain::Motion::Ports::IMotionStatePort;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionOptions;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionPlan;
using Siligen::Domain::Dispensing::ValueObjects::DispensingExecutionReport;
using Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeOverrides;
using Siligen::Domain::Dispensing::ValueObjects::DispensingRuntimeParams;

class DispensingProcessService {
   public:
    DispensingProcessService(std::shared_ptr<IValvePort> valve_port,
                             std::shared_ptr<IInterpolationPort> interpolation_port,
                             std::shared_ptr<IMotionStatePort> motion_state_port,
                             std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port,
                             std::shared_ptr<IConfigurationPort> config_port) noexcept;

    Result<void> ValidateHardwareConnection() noexcept;
    Result<DispensingRuntimeParams> BuildRuntimeParams(const DispensingRuntimeOverrides& overrides) noexcept;

    Result<DispensingExecutionReport> ExecuteProcess(const DispensingExecutionPlan& plan,
                                                     const DispensingRuntimeParams& params,
                                                     const DispensingExecutionOptions& options,
                                                     std::atomic<bool>* stop_flag,
                                                     std::atomic<bool>* pause_flag,
                                                     std::atomic<bool>* pause_applied_flag,
                                                     IDispensingExecutionObserver* observer = nullptr) noexcept;

    void StopExecution(std::atomic<bool>* stop_flag, std::atomic<bool>* pause_flag = nullptr) noexcept;

   private:
    Result<void> ConfigureCoordinateSystem(const DispensingRuntimeParams& params) noexcept;
    Result<void> WaitForMotionComplete(int32 timeout_ms,
                                       std::atomic<bool>* stop_flag,
                                       std::atomic<bool>* pause_flag,
                                       std::atomic<bool>* pause_applied_flag,
                                       const Siligen::Shared::Types::Point2D* final_target_position,
                                       float32 position_tolerance_mm,
                                       uint32 total_segments,
                                       bool dispense_enabled,
                                       IDispensingExecutionObserver* observer) noexcept;
    Result<void> WaitWhilePaused(std::atomic<bool>* stop_flag,
                                 std::atomic<bool>* pause_flag,
                                 std::atomic<bool>* pause_applied_flag,
                                 bool dispense_enabled,
                                 IDispensingExecutionObserver* observer) noexcept;
    Result<DispensingExecutionReport> ExecutePlanInternal(const DispensingExecutionPlan& plan,
                                                          const DispensingRuntimeParams& params,
                                                          const DispensingExecutionOptions& options,
                                                          std::atomic<bool>* stop_flag,
                                                          std::atomic<bool>* pause_flag,
                                                          std::atomic<bool>* pause_applied_flag,
                                                          IDispensingExecutionObserver* observer) noexcept;

    short ResolveDispenserStartLevel() const noexcept;
    bool IsStopRequested(const std::atomic<bool>* stop_flag) const noexcept;
    void PublishProgress(IDispensingExecutionObserver* observer,
                         uint32 executed_segments,
                         uint32 total_segments) const noexcept;
    void PublishPauseState(IDispensingExecutionObserver* observer, bool paused) const noexcept;

    std::shared_ptr<IValvePort> valve_port_;
    std::shared_ptr<IInterpolationPort> interpolation_port_;
    std::shared_ptr<IMotionStatePort> motion_state_port_;
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port_;
    std::shared_ptr<IConfigurationPort> config_port_;
};

}  // namespace Siligen::Domain::Dispensing::DomainServices
