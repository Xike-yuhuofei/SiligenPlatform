#include "sim/scheme_c/runtime_bridge.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "sim/scheme_c/process_runtime_shim.h"
#include "sim/scheme_c/recorder.h"

#if defined(SIM_ENGINE_SCHEME_C_HAS_PROCESS_RUNTIME_CORE)
#include "application/usecases/motion/coordination/MotionCoordinationUseCase.h"
#include "application/usecases/motion/homing/HomeAxesUseCase.h"
#include "application/usecases/motion/monitoring/MotionMonitoringUseCase.h"
#include "application/usecases/motion/ptp/MoveToPositionUseCase.h"
#include "application/usecases/motion/safety/MotionSafetyUseCase.h"
#include "domain/configuration/ports/IConfigurationPort.h"
#include "domain/motion/domain-services/MotionControlServiceImpl.h"
#include "domain/motion/domain-services/MotionStatusServiceImpl.h"
#include "domain/motion/domain-services/MotionValidationService.h"
#include "domain/motion/domain-services/interpolation/InterpolationProgramPlanner.h"
#include "domain/motion/ports/IMotionConnectionPort.h"
#include "domain/motion/ports/IPositionControlPort.h"
#include "domain/system/ports/IEventPublisherPort.h"
#include "domain/trajectory/domain-services/MotionPlanner.h"
#include "domain/trajectory/value-objects/GeometryUtils.h"
#include "shared/types/AxisTypes.h"
#include "shared/types/Error.h"
#include "shared/types/HardwareConfiguration.h"
#include "shared/types/Point.h"
#endif

namespace sim::scheme_c {
namespace {

#if defined(SIM_ENGINE_SCHEME_C_HAS_PROCESS_RUNTIME_CORE)
constexpr bool kProcessRuntimeCoreLinked = true;
#else
constexpr bool kProcessRuntimeCoreLinked = false;
#endif

std::int32_t defaultAxisIndexForName(const std::string& axis_name, std::int32_t fallback_index) {
    if (axis_name == "X") {
        return 0;
    }
    if (axis_name == "Y") {
        return 1;
    }
    if (axis_name == "Z") {
        return 2;
    }
    if (axis_name == "U") {
        return 3;
    }

    return fallback_index;
}

const char* commandKindName(RuntimeBridgeCommandKind kind) {
    switch (kind) {
    case RuntimeBridgeCommandKind::Home:
        return "home";
    case RuntimeBridgeCommandKind::Move:
        return "move";
    case RuntimeBridgeCommandKind::ExecutePath:
        return "execute_path";
    case RuntimeBridgeCommandKind::Stop:
        return "stop";
    }

    return "unknown";
}

struct CommandRecord {
    RuntimeBridgeCommandStatus status{};
    std::variant<
        RuntimeBridgeHomeCommand,
        RuntimeBridgeMoveCommand,
        RuntimeBridgePathCommand,
        RuntimeBridgeStopCommand> payload{};
};

class RuntimeBridgeSeam final : public RuntimeBridge, public RuntimeBridgeControl {
public:
    explicit RuntimeBridgeSeam(RuntimeBridgeBindings bindings)
        : bindings_(std::move(bindings)) {
        metadata_.mode = RuntimeBridgeMode::SeamStub;
        metadata_.process_runtime_core_linked = kProcessRuntimeCoreLinked;
        metadata_.next_integration_point =
            "Attach process-runtime-core motion use cases and ports behind this seam";
        metadata_.follow_up_seams = {
            "SC-NB-001 trigger controller port shim",
            "SC-NB-002 richer interpolation buffering",
            "SC-NB-004 non-blocking trajectory execution entry"
        };
    }

    RuntimeBridgeMetadata metadata() const override {
        return metadata_;
    }

    const RuntimeBridgeBindings& bindings() const override {
        return bindings_;
    }

    void attach(VirtualAxisGroup& axis_group, VirtualIo& io, Recorder* recorder) override {
        axis_group_ = &axis_group;
        io_ = &io;
        recorder_ = recorder;
    }

    void initialize(const TickInfo& tick) override {
        initialized_ = true;
        stop_requested_ = false;
        last_tick_ = tick;

        if (recorder_ != nullptr) {
            recorder_->recordEvent(
                tick,
                "RuntimeBridge",
                "Runtime bridge seam initialized with frozen axis and IO bindings.");
        }
    }

    bool advance(const TickInfo& tick) override {
        last_tick_ = tick;

        if (!initialized_ || stop_requested_) {
            return false;
        }

        if (!reported_once_ && recorder_ != nullptr) {
            recorder_->recordEvent(
                tick,
                "RuntimeBridge",
                "Approved shim bundle is the stable integration path for process-runtime-core motion ports.");
            reported_once_ = true;
        }

        return false;
    }

    void requestStop() override {
        stop_requested_ = true;
    }

    std::uint64_t submitHome(const RuntimeBridgeHomeCommand& command) override {
        return recordUnsupported(RuntimeBridgeCommandKind::Home, command);
    }

    std::uint64_t submitMove(const RuntimeBridgeMoveCommand& command) override {
        return recordUnsupported(RuntimeBridgeCommandKind::Move, command);
    }

    std::uint64_t submitPath(const RuntimeBridgePathCommand& command) override {
        return recordUnsupported(RuntimeBridgeCommandKind::ExecutePath, command);
    }

    std::uint64_t submitStop(const RuntimeBridgeStopCommand& command) override {
        return recordUnsupported(RuntimeBridgeCommandKind::Stop, command);
    }

    std::optional<RuntimeBridgeCommandStatus> commandStatus(std::uint64_t command_id) const override {
        auto it = std::find_if(
            command_history_.begin(),
            command_history_.end(),
            [command_id](const RuntimeBridgeCommandStatus& status) { return status.command_id == command_id; });
        if (it == command_history_.end()) {
            return std::nullopt;
        }

        return *it;
    }

    std::vector<RuntimeBridgeCommandStatus> commandHistory() const override {
        return command_history_;
    }

    bool hasPendingCommands() const override {
        return false;
    }

    bool hasCommandFailure() const override {
        return has_failures_;
    }

private:
    template <typename TCommand>
    std::uint64_t recordUnsupported(RuntimeBridgeCommandKind kind, const TCommand& command) {
        (void)command;

        RuntimeBridgeCommandStatus status;
        status.command_id = next_command_id_++;
        status.kind = kind;
        status.state = RuntimeBridgeCommandState::Failed;
        status.submitted_tick = last_tick_.tick_index;
        status.started_tick = last_tick_.tick_index;
        status.completed_tick = last_tick_.tick_index;
        status.detail = "process-runtime-core bridge is not linked in this build";
        command_history_.push_back(status);
        has_failures_ = true;
        return status.command_id;
    }

    RuntimeBridgeMetadata metadata_{};
    RuntimeBridgeBindings bindings_{};
    VirtualAxisGroup* axis_group_{nullptr};
    VirtualIo* io_{nullptr};
    Recorder* recorder_{nullptr};
    TickInfo last_tick_{};
    std::vector<RuntimeBridgeCommandStatus> command_history_{};
    std::uint64_t next_command_id_{1};
    bool initialized_{false};
    bool reported_once_{false};
    bool stop_requested_{false};
    bool has_failures_{false};
};

#if defined(SIM_ENGINE_SCHEME_C_HAS_PROCESS_RUNTIME_CORE)

using HomeAxesUseCase = Siligen::Application::UseCases::Motion::Homing::HomeAxesUseCase;
using HomeAxesRequest = Siligen::Application::UseCases::Motion::Homing::HomeAxesRequest;
using MotionCoordinationUseCase = Siligen::Application::UseCases::Motion::Coordination::MotionCoordinationUseCase;
using MotionMonitoringUseCase = Siligen::Application::UseCases::Motion::Monitoring::MotionMonitoringUseCase;
using MoveToPositionRequest = Siligen::Application::UseCases::Motion::PTP::MoveToPositionRequest;
using MoveToPositionUseCase = Siligen::Application::UseCases::Motion::PTP::MoveToPositionUseCase;
using MotionSafetyUseCase = Siligen::Application::UseCases::Motion::Safety::MotionSafetyUseCase;
using IConfigurationPort = Siligen::Domain::Configuration::Ports::IConfigurationPort;
using HomingConfig = Siligen::Domain::Configuration::Ports::HomingConfig;
using MotionControlServiceImpl = Siligen::Domain::Motion::DomainServices::MotionControlServiceImpl;
using MotionStatusServiceImpl = Siligen::Domain::Motion::DomainServices::MotionStatusServiceImpl;
using MotionValidationService = Siligen::Domain::Motion::DomainServices::MotionValidationService;
using InterpolationProgramPlanner = Siligen::Domain::Motion::DomainServices::InterpolationProgramPlanner;
using IMotionConnectionPort = Siligen::Domain::Motion::Ports::IMotionConnectionPort;
using IMotionStatePort = Siligen::Domain::Motion::Ports::IMotionStatePort;
using IPositionControlPort = Siligen::Domain::Motion::Ports::IPositionControlPort;
using MotionCommand = Siligen::Domain::Motion::Ports::MotionCommand;
using CoordinateSystemConfig = Siligen::Domain::Motion::Ports::CoordinateSystemConfig;
using IEventPublisherPort = Siligen::Domain::System::Ports::IEventPublisherPort;
using DomainEvent = Siligen::Domain::System::Ports::DomainEvent;
using EventType = Siligen::Domain::System::Ports::EventType;
using MotionPlanner = Siligen::Domain::Trajectory::DomainServices::MotionPlanner;
using MotionConfig = Siligen::Domain::Trajectory::ValueObjects::MotionConfig;
using ProcessPath = Siligen::Domain::Trajectory::ValueObjects::ProcessPath;
using ProcessSegment = Siligen::Domain::Trajectory::ValueObjects::ProcessSegment;
using ProcessTag = Siligen::Domain::Trajectory::ValueObjects::ProcessTag;
using SegmentType = Siligen::Domain::Trajectory::ValueObjects::SegmentType;
using Point2D = Siligen::Shared::Types::Point2D;
using Error = Siligen::Shared::Types::Error;
using ErrorCode = Siligen::Shared::Types::ErrorCode;
using HardwareConfiguration = Siligen::Shared::Types::HardwareConfiguration;
using HardwareMode = Siligen::Shared::Types::HardwareMode;
using LogicalAxisId = Siligen::Shared::Types::LogicalAxisId;
template <typename T>
using Result = Siligen::Shared::Types::Result<T>;
using ResultVoid = Siligen::Shared::Types::Result<void>;
using Siligen::Shared::Types::FromIndex;
using Siligen::Shared::Types::IsValid;
using Siligen::Shared::Types::ToIndex;

bool almostEqual(double lhs, double rhs, double epsilon = 1e-6) {
    return std::abs(lhs - rhs) <= epsilon;
}

Error makeBridgeError(ErrorCode code, const std::string& message, const std::string& source) {
    return Error(code, message, source);
}

template <typename T>
Result<T> notImplemented(const char* method, const char* source) {
    return Result<T>::Failure(makeBridgeError(ErrorCode::NOT_IMPLEMENTED, method, source));
}

ResultVoid notImplementedVoid(const char* method, const char* source) {
    return ResultVoid::Failure(makeBridgeError(ErrorCode::NOT_IMPLEMENTED, method, source));
}

class RecorderEventPublisher final : public IEventPublisherPort {
public:
    RecorderEventPublisher(Recorder*& recorder, TickInfo& tick)
        : recorder_(recorder), tick_(tick) {}

    ResultVoid Publish(const DomainEvent& event) override {
        return publish(event);
    }

    ResultVoid PublishAsync(const DomainEvent& event) override {
        return publish(event);
    }

    Result<Siligen::Shared::Types::int32> Subscribe(
        EventType /*type*/,
        Siligen::Domain::System::Ports::EventHandler /*handler*/) override {
        return Result<Siligen::Shared::Types::int32>::Success(1);
    }

    ResultVoid Unsubscribe(Siligen::Shared::Types::int32 /*subscription_id*/) override {
        return ResultVoid::Success();
    }

    ResultVoid UnsubscribeAll(EventType /*type*/) override {
        return ResultVoid::Success();
    }

    Result<std::vector<DomainEvent*>> GetEventHistory(
        EventType /*type*/,
        Siligen::Shared::Types::int32 /*max_count*/) const override {
        return Result<std::vector<DomainEvent*>>::Success({});
    }

    ResultVoid ClearEventHistory() override {
        return ResultVoid::Success();
    }

private:
    ResultVoid publish(const DomainEvent& event) {
        if (recorder_ != nullptr) {
            recorder_->recordEvent(tick_, event.source, event.message);
        }
        return ResultVoid::Success();
    }

    Recorder*& recorder_;
    TickInfo& tick_;
};

class AlwaysConnectedMotionConnectionPort final : public IMotionConnectionPort {
public:
    ResultVoid Connect(const std::string& /*card_ip*/, const std::string& /*pc_ip*/, std::int16_t /*port*/) override {
        connected_ = true;
        return ResultVoid::Success();
    }

    ResultVoid Disconnect() override {
        connected_ = false;
        return ResultVoid::Success();
    }

    Result<bool> IsConnected() const override {
        return Result<bool>::Success(connected_);
    }

    ResultVoid Reset() override {
        connected_ = true;
        return ResultVoid::Success();
    }

    Result<std::string> GetCardInfo() const override {
        return Result<std::string>::Success("virtual-process-runtime-core-bridge");
    }

private:
    bool connected_{true};
};

class RuntimeConfigurationPort final : public IConfigurationPort {
public:
    explicit RuntimeConfigurationPort(std::size_t axis_count) {
        const auto configured_axis_count = static_cast<Siligen::Shared::Types::int32>(
            std::max<std::size_t>(1, axis_count));
        hardware_config_.num_axes = configured_axis_count;
        hardware_config_.max_velocity_mm_s = 200.0f;
        hardware_config_.max_acceleration_mm_s2 = 1000.0f;
        hardware_config_.max_deceleration_mm_s2 = 1000.0f;
        hardware_config_.soft_limit_negative_mm = -1000.0f;
        hardware_config_.soft_limit_positive_mm = 1000.0f;

        homing_configs_.reserve(static_cast<std::size_t>(configured_axis_count));
        for (Siligen::Shared::Types::int32 axis = 0; axis < configured_axis_count; ++axis) {
            HomingConfig config;
            config.axis = axis;
            config.rapid_velocity = 50.0f;
            config.locate_velocity = 20.0f;
            config.acceleration = 500.0f;
            config.deceleration = 500.0f;
            homing_configs_.push_back(config);
        }
    }

    Result<Siligen::Domain::Configuration::Ports::SystemConfig> LoadConfiguration() override {
        return notImplemented<Siligen::Domain::Configuration::Ports::SystemConfig>(
            "LoadConfiguration not required by runtime bridge",
            "RuntimeConfigurationPort");
    }

    ResultVoid SaveConfiguration(const Siligen::Domain::Configuration::Ports::SystemConfig& /*config*/) override {
        return notImplementedVoid("SaveConfiguration not required by runtime bridge", "RuntimeConfigurationPort");
    }

    ResultVoid ReloadConfiguration() override {
        return notImplementedVoid("ReloadConfiguration not required by runtime bridge", "RuntimeConfigurationPort");
    }

    Result<Siligen::Domain::Configuration::Ports::DispensingConfig> GetDispensingConfig() const override {
        return notImplemented<Siligen::Domain::Configuration::Ports::DispensingConfig>(
            "GetDispensingConfig not required by runtime bridge",
            "RuntimeConfigurationPort");
    }

    ResultVoid SetDispensingConfig(const Siligen::Domain::Configuration::Ports::DispensingConfig& /*config*/) override {
        return notImplementedVoid("SetDispensingConfig not required by runtime bridge", "RuntimeConfigurationPort");
    }

    Result<Siligen::Domain::Configuration::Ports::DxfPreprocessConfig> GetDxfPreprocessConfig() const override {
        return Result<Siligen::Domain::Configuration::Ports::DxfPreprocessConfig>::Success({});
    }

    Result<Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig> GetDxfTrajectoryConfig() const override {
        return Result<Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig>::Success({});
    }

    Result<Siligen::Shared::Types::DiagnosticsConfig> GetDiagnosticsConfig() const override {
        return notImplemented<Siligen::Shared::Types::DiagnosticsConfig>(
            "GetDiagnosticsConfig not required by runtime bridge",
            "RuntimeConfigurationPort");
    }

    Result<Siligen::Domain::Configuration::Ports::MachineConfig> GetMachineConfig() const override {
        return notImplemented<Siligen::Domain::Configuration::Ports::MachineConfig>(
            "GetMachineConfig not required by runtime bridge",
            "RuntimeConfigurationPort");
    }

    ResultVoid SetMachineConfig(const Siligen::Domain::Configuration::Ports::MachineConfig& /*config*/) override {
        return notImplementedVoid("SetMachineConfig not required by runtime bridge", "RuntimeConfigurationPort");
    }

    Result<HomingConfig> GetHomingConfig(int axis) const override {
        if (axis < 0 || static_cast<std::size_t>(axis) >= homing_configs_.size()) {
            return Result<HomingConfig>::Failure(
                makeBridgeError(ErrorCode::INVALID_PARAMETER, "Homing config not found", "RuntimeConfigurationPort"));
        }
        return Result<HomingConfig>::Success(homing_configs_[static_cast<std::size_t>(axis)]);
    }

    ResultVoid SetHomingConfig(int axis, const HomingConfig& config) override {
        if (axis < 0 || static_cast<std::size_t>(axis) >= homing_configs_.size()) {
            return ResultVoid::Failure(
                makeBridgeError(ErrorCode::INVALID_PARAMETER, "Homing config out of range", "RuntimeConfigurationPort"));
        }
        homing_configs_[static_cast<std::size_t>(axis)] = config;
        return ResultVoid::Success();
    }

    Result<std::vector<HomingConfig>> GetAllHomingConfigs() const override {
        return Result<std::vector<HomingConfig>>::Success(homing_configs_);
    }

    Result<Siligen::Domain::Configuration::Ports::ValveSupplyConfig> GetValveSupplyConfig() const override {
        return notImplemented<Siligen::Domain::Configuration::Ports::ValveSupplyConfig>(
            "GetValveSupplyConfig not required by runtime bridge",
            "RuntimeConfigurationPort");
    }

    Result<Siligen::Shared::Types::DispenserValveConfig> GetDispenserValveConfig() const override {
        return notImplemented<Siligen::Shared::Types::DispenserValveConfig>(
            "GetDispenserValveConfig not required by runtime bridge",
            "RuntimeConfigurationPort");
    }

    Result<Siligen::Shared::Types::ValveCoordinationConfig> GetValveCoordinationConfig() const override {
        return notImplemented<Siligen::Shared::Types::ValveCoordinationConfig>(
            "GetValveCoordinationConfig not required by runtime bridge",
            "RuntimeConfigurationPort");
    }

    Result<Siligen::Shared::Types::VelocityTraceConfig> GetVelocityTraceConfig() const override {
        return notImplemented<Siligen::Shared::Types::VelocityTraceConfig>(
            "GetVelocityTraceConfig not required by runtime bridge",
            "RuntimeConfigurationPort");
    }

    Result<bool> ValidateConfiguration() const override {
        return Result<bool>::Success(true);
    }

    Result<std::vector<std::string>> GetValidationErrors() const override {
        return Result<std::vector<std::string>>::Success({});
    }

    ResultVoid BackupConfiguration(const std::string& /*backup_path*/) override {
        return notImplementedVoid("BackupConfiguration not required by runtime bridge", "RuntimeConfigurationPort");
    }

    ResultVoid RestoreConfiguration(const std::string& /*backup_path*/) override {
        return notImplementedVoid("RestoreConfiguration not required by runtime bridge", "RuntimeConfigurationPort");
    }

    Result<HardwareMode> GetHardwareMode() const override {
        return Result<HardwareMode>::Success(HardwareMode::Mock);
    }

    Result<HardwareConfiguration> GetHardwareConfiguration() const override {
        return Result<HardwareConfiguration>::Success(hardware_config_);
    }

private:
    HardwareConfiguration hardware_config_{};
    std::vector<HomingConfig> homing_configs_{};
};

class BridgePositionControlPort final : public IPositionControlPort {
public:
    BridgePositionControlPort(
        std::shared_ptr<Siligen::Domain::Motion::Ports::IInterpolationPort> interpolation_port,
        std::shared_ptr<IMotionStatePort> motion_state_port,
        std::vector<LogicalAxisId> available_axes)
        : interpolation_port_(std::move(interpolation_port)),
          motion_state_port_(std::move(motion_state_port)),
          available_axes_(std::move(available_axes)) {}

    ResultVoid MoveToPosition(const Point2D& position, Siligen::Shared::Types::float32 velocity) override {
        std::vector<MotionCommand> commands;
        if (hasAxis(LogicalAxisId::X)) {
            commands.push_back(MotionCommand{LogicalAxisId::X, position.x, velocity, false});
        }
        if (hasAxis(LogicalAxisId::Y)) {
            commands.push_back(MotionCommand{LogicalAxisId::Y, position.y, velocity, false});
        }
        return SynchronizedMove(commands);
    }

    ResultVoid MoveAxisToPosition(LogicalAxisId axis,
                                  Siligen::Shared::Types::float32 position,
                                  Siligen::Shared::Types::float32 velocity) override {
        return SynchronizedMove({MotionCommand{axis, position, velocity, false}});
    }

    ResultVoid RelativeMove(LogicalAxisId axis,
                            Siligen::Shared::Types::float32 distance,
                            Siligen::Shared::Types::float32 velocity) override {
        if (!motion_state_port_) {
            return ResultVoid::Failure(
                makeBridgeError(ErrorCode::PORT_NOT_INITIALIZED, "Motion state port not initialized", "BridgePositionControlPort"));
        }

        const auto current = motion_state_port_->GetAxisPosition(axis);
        if (current.IsError()) {
            return ResultVoid::Failure(current.GetError());
        }

        return MoveAxisToPosition(axis, current.Value() + distance, velocity);
    }

    ResultVoid SynchronizedMove(const std::vector<MotionCommand>& commands) override {
        if (!interpolation_port_) {
            return ResultVoid::Failure(
                makeBridgeError(ErrorCode::PORT_NOT_INITIALIZED, "Interpolation port not initialized", "BridgePositionControlPort"));
        }
        if (commands.empty()) {
            return ResultVoid::Failure(
                makeBridgeError(ErrorCode::INVALID_PARAMETER, "Motion command list is empty", "BridgePositionControlPort"));
        }

        CoordinateSystemConfig config;
        config.dimension = static_cast<std::int16_t>(commands.size());
        config.max_velocity = 0.0f;
        config.max_acceleration = 0.0f;
        config.axis_map.reserve(commands.size());

        Siligen::Domain::Motion::Ports::InterpolationData data;
        data.positions.reserve(commands.size());

        for (const auto& command : commands) {
            if (!hasAxis(command.axis)) {
                return ResultVoid::Failure(
                    makeBridgeError(ErrorCode::INVALID_PARAMETER, "Axis not available in runtime bridge", "BridgePositionControlPort"));
            }

            config.axis_map.push_back(command.axis);
            config.max_velocity = std::max(config.max_velocity, command.velocity);
            config.max_acceleration = std::max(config.max_acceleration, command.velocity);

            float target_position = command.position;
            if (command.relative) {
                const auto current = motion_state_port_->GetAxisPosition(command.axis);
                if (current.IsError()) {
                    return ResultVoid::Failure(current.GetError());
                }
                target_position += current.Value();
            }

            data.positions.push_back(target_position);
            data.velocity = std::max(data.velocity, command.velocity);
            data.acceleration = std::max(data.acceleration, command.velocity);
        }

        auto result = interpolation_port_->ConfigureCoordinateSystem(1, config);
        if (result.IsError()) {
            return result;
        }
        result = interpolation_port_->ClearInterpolationBuffer(1);
        if (result.IsError()) {
            return result;
        }
        result = interpolation_port_->AddInterpolationData(1, data);
        if (result.IsError()) {
            return result;
        }
        result = interpolation_port_->FlushInterpolationData(1);
        if (result.IsError()) {
            return result;
        }
        return interpolation_port_->StartCoordinateSystemMotion(1U);
    }

    ResultVoid StopAxis(LogicalAxisId /*axis*/, bool immediate) override {
        return StopAllAxes(immediate);
    }

    ResultVoid StopAllAxes(bool /*immediate*/) override {
        if (!interpolation_port_) {
            return ResultVoid::Failure(
                makeBridgeError(ErrorCode::PORT_NOT_INITIALIZED, "Interpolation port not initialized", "BridgePositionControlPort"));
        }
        return interpolation_port_->StopCoordinateSystemMotion(0xFFFFU);
    }

    ResultVoid EmergencyStop() override {
        return StopAllAxes(true);
    }

    ResultVoid WaitForMotionComplete(LogicalAxisId axis, Siligen::Shared::Types::int32 /*timeout_ms*/) override {
        if (!motion_state_port_) {
            return ResultVoid::Failure(
                makeBridgeError(ErrorCode::PORT_NOT_INITIALIZED, "Motion state port not initialized", "BridgePositionControlPort"));
        }

        const auto moving = motion_state_port_->IsAxisMoving(axis);
        if (moving.IsError()) {
            return ResultVoid::Failure(moving.GetError());
        }
        if (moving.Value()) {
            return ResultVoid::Failure(
                makeBridgeError(ErrorCode::TIMEOUT,
                                "Deterministic bridge wait requires later virtual ticks",
                                "BridgePositionControlPort"));
        }

        return ResultVoid::Success();
    }

private:
    bool hasAxis(LogicalAxisId axis) const {
        return std::find(available_axes_.begin(), available_axes_.end(), axis) != available_axes_.end();
    }

    std::shared_ptr<Siligen::Domain::Motion::Ports::IInterpolationPort> interpolation_port_;
    std::shared_ptr<IMotionStatePort> motion_state_port_;
    std::vector<LogicalAxisId> available_axes_{};
};

class RuntimeMotionValidationService final : public MotionValidationService {
public:
    explicit RuntimeMotionValidationService(std::shared_ptr<IMotionStatePort> motion_state_port)
        : motion_state_port_(std::move(motion_state_port)) {}

    ResultVoid ValidatePosition(const Point2D& position) const override {
        if (!std::isfinite(position.x) || !std::isfinite(position.y)) {
            return ResultVoid::Failure(
                makeBridgeError(ErrorCode::INVALID_PARAMETER, "Target position is not finite", "RuntimeMotionValidationService"));
        }
        return ResultVoid::Success();
    }

    ResultVoid ValidateSpeed(float speed) const override {
        if (!std::isfinite(speed) || speed <= 0.0f) {
            return ResultVoid::Failure(
                makeBridgeError(ErrorCode::INVALID_PARAMETER, "Speed must be positive", "RuntimeMotionValidationService"));
        }
        return ResultVoid::Success();
    }

    Result<bool> IsPositionErrorExceeded(const Point2D& target, const Point2D& actual) const override {
        return Result<bool>::Success(target.DistanceTo(actual) > 0.01f);
    }

private:
    std::shared_ptr<IMotionStatePort> motion_state_port_;
};

struct ActiveHomeCommand {
    std::vector<LogicalAxisId> axes{};
};

struct ActiveMoveCommand {
    double target_x_mm{0.0};
    double target_y_mm{0.0};
};

struct ActivePathCommand {
    std::vector<Siligen::Domain::Motion::Ports::InterpolationData> program{};
    std::size_t next_segment_index{0};
    bool segment_in_flight{false};
};

struct ActiveStopCommand {
    bool immediate{false};
};

using ActiveCommandState = std::variant<
    ActiveHomeCommand,
    ActiveMoveCommand,
    ActivePathCommand,
    ActiveStopCommand>;

struct ActiveCommand {
    std::uint64_t command_id{0};
    RuntimeBridgeCommandKind kind{RuntimeBridgeCommandKind::Home};
    ActiveCommandState state{};
};

class ProcessRuntimeCoreRuntimeBridge final : public RuntimeBridge, public RuntimeBridgeControl {
public:
    explicit ProcessRuntimeCoreRuntimeBridge(RuntimeBridgeBindings bindings)
        : bindings_(std::move(bindings)),
          event_publisher_(recorder_, last_tick_) {
        metadata_.mode = RuntimeBridgeMode::ProcessRuntimeCore;
        metadata_.process_runtime_core_linked = true;
        metadata_.next_integration_point =
            "Close remaining non-blocking seams around richer path execution and trigger integration";
        metadata_.follow_up_seams = {
            "SC-NB-001 trigger controller port shim",
            "SC-NB-002 richer interpolation buffering",
            "SC-NB-004 non-blocking trajectory execution entry"
        };
    }

    RuntimeBridgeMetadata metadata() const override {
        return metadata_;
    }

    const RuntimeBridgeBindings& bindings() const override {
        return bindings_;
    }

    void attach(VirtualAxisGroup& axis_group, VirtualIo& io, Recorder* recorder) override;
    void initialize(const TickInfo& tick) override;
    bool advance(const TickInfo& tick) override;
    void requestStop() override;

    std::uint64_t submitHome(const RuntimeBridgeHomeCommand& command) override;
    std::uint64_t submitMove(const RuntimeBridgeMoveCommand& command) override;
    std::uint64_t submitPath(const RuntimeBridgePathCommand& command) override;
    std::uint64_t submitStop(const RuntimeBridgeStopCommand& command) override;

    std::optional<RuntimeBridgeCommandStatus> commandStatus(std::uint64_t command_id) const override;
    std::vector<RuntimeBridgeCommandStatus> commandHistory() const override;
    bool hasPendingCommands() const override;
    bool hasCommandFailure() const override;

private:
    template <typename TCommand>
    std::uint64_t enqueueCommand(RuntimeBridgeCommandKind kind, const TCommand& command);

    CommandRecord* findCommand(std::uint64_t command_id);
    const CommandRecord* findCommand(std::uint64_t command_id) const;
    void recordEvent(const TickInfo& tick, const std::string& source, const std::string& message);
    void markCommandStarted(CommandRecord& record, const TickInfo& tick, const std::string& detail);
    void markCommandCompleted(CommandRecord& record,
                              RuntimeBridgeCommandState state,
                              const TickInfo& tick,
                              const std::string& detail);
    void cancelPendingCommands(const TickInfo& tick, const std::string& detail);
    void failActiveCommand(const TickInfo& tick, const std::string& detail);
    std::vector<LogicalAxisId> axesForHomeCommand(const RuntimeBridgeHomeCommand& command) const;
    bool axesHomed(const std::vector<LogicalAxisId>& axes) const;
    bool moveReachedTarget(const ActiveMoveCommand& move) const;
    ResultVoid dispatchPathSegment(ActivePathCommand& path_state);
    bool anyAxisMoving() const;
    Point2D currentPosition() const;
    Result<ActivePathCommand> buildPathState(const RuntimeBridgePathCommand& command) const;
    void startNextCommand(const TickInfo& tick);
    void pollActiveCommand(const TickInfo& tick);

    RuntimeBridgeMetadata metadata_{};
    RuntimeBridgeBindings bindings_{};
    ProcessRuntimeCoreShimBundle shim_bundle_{};
    VirtualAxisGroup* axis_group_{nullptr};
    VirtualIo* io_{nullptr};
    Recorder* recorder_{nullptr};
    TickInfo last_tick_{};
    std::vector<LogicalAxisId> available_axes_{};

    RecorderEventPublisher event_publisher_;
    std::shared_ptr<RuntimeConfigurationPort> configuration_port_{};
    std::shared_ptr<AlwaysConnectedMotionConnectionPort> motion_connection_port_{};
    std::shared_ptr<BridgePositionControlPort> position_control_port_{};
    std::shared_ptr<RuntimeMotionValidationService> motion_validation_service_{};
    std::shared_ptr<MotionControlServiceImpl> motion_control_service_{};
    std::shared_ptr<MotionStatusServiceImpl> motion_status_service_{};

    std::unique_ptr<HomeAxesUseCase> home_use_case_{};
    std::unique_ptr<MoveToPositionUseCase> move_use_case_{};
    std::unique_ptr<MotionCoordinationUseCase> coordination_use_case_{};
    std::unique_ptr<MotionMonitoringUseCase> monitoring_use_case_{};
    std::unique_ptr<MotionSafetyUseCase> safety_use_case_{};
    std::unique_ptr<MotionPlanner> motion_planner_{};
    std::unique_ptr<InterpolationProgramPlanner> interpolation_program_planner_{};

    std::deque<CommandRecord> commands_{};
    std::deque<std::uint64_t> pending_command_ids_{};
    std::optional<ActiveCommand> active_command_{};
    std::uint64_t next_command_id_{1};
    bool initialized_{false};
    bool stop_requested_{false};
    bool command_failure_{false};
};

#endif

}  // namespace

RuntimeBridgeControl* runtimeBridgeControl(RuntimeBridge& bridge) noexcept {
    return dynamic_cast<RuntimeBridgeControl*>(&bridge);
}

const RuntimeBridgeControl* runtimeBridgeControl(const RuntimeBridge& bridge) noexcept {
    return dynamic_cast<const RuntimeBridgeControl*>(&bridge);
}

RuntimeBridgeBindings makeDefaultRuntimeBridgeBindings(
    const std::vector<std::string>& axis_names,
    const std::vector<std::string>& io_channels) {
    RuntimeBridgeBindings bindings;

    for (std::size_t index = 0; index < axis_names.size(); ++index) {
        bindings.axis_bindings.push_back(RuntimeAxisBinding{
            defaultAxisIndexForName(axis_names[index], static_cast<std::int32_t>(index)),
            axis_names[index]
        });
    }

    for (std::size_t index = 0; index < io_channels.size(); ++index) {
        bindings.io_bindings.push_back(RuntimeIoBinding{
            static_cast<std::int32_t>(index),
            io_channels[index],
            true
        });
    }

    return bindings;
}

std::unique_ptr<RuntimeBridge> createRuntimeBridge(
    const RuntimeBridgeBindings& bindings) {
#if defined(SIM_ENGINE_SCHEME_C_HAS_PROCESS_RUNTIME_CORE)
    return std::make_unique<ProcessRuntimeCoreRuntimeBridge>(bindings);
#else
    return createRuntimeBridgeSeam(bindings);
#endif
}

std::unique_ptr<RuntimeBridge> createRuntimeBridgeSeam(
    const RuntimeBridgeBindings& bindings) {
    return std::make_unique<RuntimeBridgeSeam>(bindings);
}

}  // namespace sim::scheme_c
