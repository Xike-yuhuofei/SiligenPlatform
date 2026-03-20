#include "sim/scheme_c/runtime_bridge.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "sim/scheme_c/recorder.h"
#include "sim/scheme_c/virtual_io.h"

#if defined(SIM_ENGINE_SCHEME_C_HAS_PROCESS_RUNTIME_CORE)
#include "application/usecases/motion/coordination/MotionCoordinationUseCase.h"
#include "application/usecases/motion/homing/HomeAxesUseCase.h"
#include "application/usecases/motion/monitoring/MotionMonitoringUseCase.h"
#include "application/usecases/motion/ptp/MoveToPositionUseCase.h"
#include "application/usecases/motion/runtime/MotionRuntimeAssemblyFactory.h"
#include "application/usecases/motion/safety/MotionSafetyUseCase.h"
#include "application/usecases/motion/trajectory/DeterministicPathExecutionUseCase.h"
#include "domain/configuration/ports/IConfigurationPort.h"
#include "domain/motion/ports/IMotionRuntimePort.h"
#include "domain/system/ports/IEventPublisherPort.h"
#include "process_runtime_ports.h"
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
            "Attach process-runtime-core motion use cases and remaining trigger integration behind this seam";
        metadata_.follow_up_seams = {
            "SC-NB-001 trigger controller port shim"
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
using MotionIOCommand = Siligen::Application::UseCases::Motion::Coordination::MotionIOCommand;
using MotionMonitoringUseCase = Siligen::Application::UseCases::Motion::Monitoring::MotionMonitoringUseCase;
using MoveToPositionRequest = Siligen::Application::UseCases::Motion::PTP::MoveToPositionRequest;
using MoveToPositionUseCase = Siligen::Application::UseCases::Motion::PTP::MoveToPositionUseCase;
using MotionRuntimeAssemblyDependencies =
    Siligen::Application::UseCases::Motion::Runtime::MotionRuntimeAssemblyDependencies;
using MotionRuntimeAssemblyFactory = Siligen::Application::UseCases::Motion::Runtime::MotionRuntimeAssemblyFactory;
using MotionSafetyUseCase = Siligen::Application::UseCases::Motion::Safety::MotionSafetyUseCase;
using DeterministicPathExecutionRequest = Siligen::Application::UseCases::Motion::Trajectory::DeterministicPathExecutionRequest;
using DeterministicPathExecutionState = Siligen::Application::UseCases::Motion::Trajectory::DeterministicPathExecutionState;
using DeterministicPathExecutionStatus = Siligen::Application::UseCases::Motion::Trajectory::DeterministicPathExecutionStatus;
using DeterministicPathExecutionUseCase = Siligen::Application::UseCases::Motion::Trajectory::DeterministicPathExecutionUseCase;
using DeterministicPathSegment = Siligen::Application::UseCases::Motion::Trajectory::DeterministicPathSegment;
using DeterministicPathSegmentType = Siligen::Application::UseCases::Motion::Trajectory::DeterministicPathSegmentType;
using IConfigurationPort = Siligen::Domain::Configuration::Ports::IConfigurationPort;
using HomingConfig = Siligen::Domain::Configuration::Ports::HomingConfig;
using IMotionStatePort = Siligen::Domain::Motion::Ports::IMotionStatePort;
using IEventPublisherPort = Siligen::Domain::System::Ports::IEventPublisherPort;
using DomainEvent = Siligen::Domain::System::Ports::DomainEvent;
using EventType = Siligen::Domain::System::Ports::EventType;
using ITriggerControllerPort = Siligen::Domain::Dispensing::Ports::ITriggerControllerPort;
using TriggerConfig = Siligen::Domain::Dispensing::Ports::TriggerConfig;
using TriggerMode = Siligen::Domain::Dispensing::Ports::TriggerMode;
using TriggerStatus = Siligen::Domain::Dispensing::Ports::TriggerStatus;
using Point2D = Siligen::Shared::Types::Point2D;
using Error = Siligen::Shared::Types::Error;
using ErrorCode = Siligen::Shared::Types::ErrorCode;
using HardwareConfiguration = Siligen::Shared::Types::HardwareConfiguration;
using HardwareMode = Siligen::Shared::Types::HardwareMode;
using LogicalAxisId = Siligen::Shared::Types::LogicalAxisId;
using SignalType = Siligen::Domain::Dispensing::Ports::SignalType;
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

struct ActiveHomeCommand {
    std::vector<LogicalAxisId> axes{};
};

struct ActiveMoveCommand {
    double target_x_mm{0.0};
    double target_y_mm{0.0};
};

struct ActivePathCommand {};

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

Duration durationFromMicroseconds(std::int32_t microseconds) {
    if (microseconds <= 0) {
        return Duration::zero();
    }

    return std::chrono::duration_cast<Duration>(std::chrono::microseconds(microseconds));
}

std::string boolText(bool value) {
    return value ? "true" : "false";
}

std::string replayTriggerSpaceName(ReplayTriggerSpace space) {
    switch (space) {
    case ReplayTriggerSpace::AxisPosition:
        return "axis_position";
    case ReplayTriggerSpace::PathProgress:
        return "path_progress";
    }

    return "unknown";
}

class DeterministicIoReplayController final {
public:
    DeterministicIoReplayController(VirtualIo*& io, Recorder*& recorder)
        : io_(io), recorder_(recorder) {}

    void configure(const DeterministicReplayPlan& replay_plan) {
        configured_delays_.clear();
        for (const auto& spec : replay_plan.io_delays) {
            configured_delays_[spec.channel] = spec.delay;
        }
        valve_spec_ = replay_plan.valve;
    }

    void reset() {
        desired_io_states_.clear();
        applied_io_states_.clear();
        pending_io_commands_.clear();
        pending_valve_changes_.clear();
        next_sequence_ = 0;

        valve_actual_state_ = false;
        valve_commanded_state_ = false;
        if (valve_spec_.has_value() && io_ != nullptr) {
            valve_actual_state_ = io_->readSignal(valve_spec_->channel);
            valve_commanded_state_ = valve_actual_state_;
            desired_io_states_[valve_spec_->channel] = valve_actual_state_;
            applied_io_states_[valve_spec_->channel] = valve_actual_state_;
        }
    }

    void commandOutput(const TickInfo& tick,
                       const std::string& channel,
                       bool state,
                       const std::string& source,
                       Duration extra_delay = Duration::zero()) {
        if (channel.empty()) {
            return;
        }

        if (desiredState(channel) == state) {
            return;
        }

        desired_io_states_[channel] = state;
        insertPending(
            pending_io_commands_,
            PendingIoCommand{
                channel,
                state,
                tick.now + delayForChannel(channel) + std::max(extra_delay, Duration::zero()),
                source,
                next_sequence_++
            });
    }

    void advance(const TickInfo& tick) {
        bool valve_command_dirty = false;

        while (!pending_io_commands_.empty() && pending_io_commands_.front().due_time <= tick.now) {
            const auto command = pending_io_commands_.front();
            pending_io_commands_.erase(pending_io_commands_.begin());

            if (appliedState(command.channel) == command.state) {
                continue;
            }

            applied_io_states_[command.channel] = command.state;
            recordEvent(
                tick,
                "Replay",
                "Applied IO replay " + command.channel + "=" + boolText(command.state) +
                    " from " + command.source + ".");

            if (isValveChannel(command.channel)) {
                valve_command_dirty = true;
                continue;
            }

            if (io_ != nullptr) {
                io_->writeOutput(command.channel, command.state);
            }
        }

        if (valve_command_dirty && valve_spec_.has_value()) {
            setValveCommand(appliedState(valve_spec_->channel), tick);
        }

        while (!pending_valve_changes_.empty() && pending_valve_changes_.front().due_time <= tick.now) {
            const auto change = pending_valve_changes_.front();
            pending_valve_changes_.erase(pending_valve_changes_.begin());

            if (valve_actual_state_ == change.state) {
                continue;
            }

            valve_actual_state_ = change.state;
            if (io_ != nullptr && valve_spec_.has_value()) {
                io_->writeOutput(valve_spec_->channel, change.state);
            }

            if (valve_spec_.has_value()) {
                recordEvent(
                    tick,
                    "Replay",
                    "Applied valve replay " + valve_spec_->channel + "=" + boolText(change.state) + ".");
            }
        }
    }

    void clearPending(const TickInfo& tick, const std::string& reason) {
        const bool had_pending = hasPendingWork();
        pending_io_commands_.clear();
        pending_valve_changes_.clear();
        for (const auto& entry : applied_io_states_) {
            desired_io_states_[entry.first] = entry.second;
        }
        if (valve_spec_.has_value()) {
            valve_commanded_state_ = valve_actual_state_;
            desired_io_states_[valve_spec_->channel] = valve_actual_state_;
            applied_io_states_[valve_spec_->channel] = valve_actual_state_;
        }
        if (had_pending) {
            recordEvent(tick, "Replay", reason);
        }
    }

    bool hasPendingWork() const {
        return !pending_io_commands_.empty() || !pending_valve_changes_.empty();
    }

private:
    struct PendingIoCommand {
        std::string channel;
        bool state{false};
        Duration due_time{Duration::zero()};
        std::string source;
        std::size_t sequence{0};
    };

    struct PendingValveChange {
        bool state{false};
        Duration due_time{Duration::zero()};
        std::size_t sequence{0};
    };

    template <typename TPending>
    void insertPending(std::vector<TPending>& queue, TPending pending) {
        queue.push_back(std::move(pending));
        std::stable_sort(
            queue.begin(),
            queue.end(),
            [](const TPending& lhs, const TPending& rhs) {
                if (lhs.due_time != rhs.due_time) {
                    return lhs.due_time < rhs.due_time;
                }
                return lhs.sequence < rhs.sequence;
            });
    }

    Duration delayForChannel(const std::string& channel) const {
        const auto it = configured_delays_.find(channel);
        return it == configured_delays_.end() ? Duration::zero() : it->second;
    }

    bool isValveChannel(const std::string& channel) const {
        return valve_spec_.has_value() && valve_spec_->channel == channel;
    }

    bool appliedState(const std::string& channel) {
        auto it = applied_io_states_.find(channel);
        if (it != applied_io_states_.end()) {
            return it->second;
        }

        const auto state = io_ != nullptr ? io_->readSignal(channel) : false;
        applied_io_states_[channel] = state;
        desired_io_states_.try_emplace(channel, state);
        return state;
    }

    bool desiredState(const std::string& channel) {
        auto it = desired_io_states_.find(channel);
        if (it != desired_io_states_.end()) {
            return it->second;
        }

        const auto state = appliedState(channel);
        desired_io_states_[channel] = state;
        return state;
    }

    void setValveCommand(bool state, const TickInfo& tick) {
        if (!valve_spec_.has_value()) {
            return;
        }

        if (valve_commanded_state_ == state) {
            return;
        }

        valve_commanded_state_ = state;
        insertPending(
            pending_valve_changes_,
            PendingValveChange{
                state,
                tick.now + (state ? valve_spec_->open_delay : valve_spec_->close_delay),
                next_sequence_++
            });
    }

    void recordEvent(const TickInfo& tick, const std::string& source, const std::string& message) {
        if (recorder_ != nullptr) {
            recorder_->recordEvent(tick, source, message);
        }
    }

    VirtualIo*& io_;
    Recorder*& recorder_;
    std::unordered_map<std::string, Duration> configured_delays_{};
    std::unordered_map<std::string, bool> desired_io_states_{};
    std::unordered_map<std::string, bool> applied_io_states_{};
    std::vector<PendingIoCommand> pending_io_commands_{};
    std::vector<PendingValveChange> pending_valve_changes_{};
    std::optional<ReplayValveSpec> valve_spec_{};
    bool valve_actual_state_{false};
    bool valve_commanded_state_{false};
    std::size_t next_sequence_{0};
};

class DeterministicTriggerControllerAdapter final : public ITriggerControllerPort {
public:
    DeterministicTriggerControllerAdapter(std::shared_ptr<IMotionStatePort> motion_state_port,
                                          std::shared_ptr<DeterministicIoReplayController> io_replay,
                                          Recorder*& recorder,
                                          TickInfo& tick,
                                          RuntimeBridgeBindings bindings)
        : motion_state_port_(std::move(motion_state_port)),
          io_replay_(std::move(io_replay)),
          recorder_(recorder),
          tick_(tick),
          bindings_(std::move(bindings)) {
        for (const auto& binding : bindings_.io_bindings) {
            signal_by_channel_[binding.channel] = binding.signal_name;
        }
    }

    ResultVoid ConfigureTrigger(const TriggerConfig& config) override {
        last_config_ = config;
        return ResultVoid::Success();
    }

    Result<TriggerConfig> GetTriggerConfig() const override {
        return Result<TriggerConfig>::Success(last_config_);
    }

    ResultVoid SetSingleTrigger(LogicalAxisId axis, float32 position, int32 pulse_width_us) override {
        const auto signal = signalNameForChannel(last_config_.channel);
        if (signal.IsError()) {
            return ResultVoid::Failure(signal.GetError());
        }

        armed_triggers_.push_back(ArmedTrigger{
            pending_space_,
            axis,
            static_cast<double>(position),
            last_config_.channel,
            signal.Value(),
            pending_output_state_,
            pulse_width_us > 0 ? static_cast<std::uint32_t>(pulse_width_us) : 0U,
            true,
            0,
            0.0,
            pending_source_
        });

        recordEvent(
            tick_,
            "TriggerController",
            "Armed compare output " + signal.Value() + "=" + boolText(pending_output_state_) +
                " at " + std::to_string(position) + " mm using " +
                replayTriggerSpaceName(pending_space_) + ".");

        pending_output_state_ = true;
        pending_space_ = ReplayTriggerSpace::AxisPosition;
        pending_source_ = "compare_output";
        return ResultVoid::Success();
    }

    ResultVoid SetContinuousTrigger(LogicalAxisId /*axis*/,
                                    float32 /*start_pos*/,
                                    float32 /*end_pos*/,
                                    float32 /*interval*/,
                                    int32 /*pulse_width_us*/) override {
        return notImplementedVoid("SetContinuousTrigger not required by runtime bridge", "TriggerControllerAdapter");
    }

    ResultVoid SetRangeTrigger(LogicalAxisId /*axis*/,
                               float32 /*start_pos*/,
                               float32 /*end_pos*/,
                               int32 /*pulse_width_us*/) override {
        return notImplementedVoid("SetRangeTrigger not required by runtime bridge", "TriggerControllerAdapter");
    }

    ResultVoid SetSequenceTrigger(LogicalAxisId axis,
                                  const std::vector<float32>& positions,
                                  int32 pulse_width_us) override {
        for (const auto position : positions) {
            auto result = SetSingleTrigger(axis, position, pulse_width_us);
            if (result.IsError()) {
                return result;
            }
        }

        return ResultVoid::Success();
    }

    ResultVoid EnableTrigger(LogicalAxisId axis) override {
        for (auto& trigger : armed_triggers_) {
            if (trigger.axis == axis) {
                trigger.enabled = true;
            }
        }
        return ResultVoid::Success();
    }

    ResultVoid DisableTrigger(LogicalAxisId axis) override {
        for (auto& trigger : armed_triggers_) {
            if (trigger.axis == axis) {
                trigger.enabled = false;
            }
        }
        return ResultVoid::Success();
    }

    ResultVoid ClearTrigger(LogicalAxisId axis) override {
        armed_triggers_.erase(
            std::remove_if(
                armed_triggers_.begin(),
                armed_triggers_.end(),
                [axis](const ArmedTrigger& trigger) { return trigger.axis == axis; }),
            armed_triggers_.end());
        return ResultVoid::Success();
    }

    Result<TriggerStatus> GetTriggerStatus(LogicalAxisId axis) const override {
        TriggerStatus status;
        for (const auto& trigger : armed_triggers_) {
            if (trigger.axis != axis) {
                continue;
            }

            status.is_enabled = status.is_enabled || trigger.enabled;
            status.trigger_count += trigger.trigger_count;
            if (trigger.trigger_count > 0) {
                status.last_trigger_position = static_cast<float32>(trigger.last_trigger_position);
            }
        }
        return Result<TriggerStatus>::Success(status);
    }

    Result<bool> IsTriggerEnabled(LogicalAxisId axis) const override {
        for (const auto& trigger : armed_triggers_) {
            if (trigger.axis == axis && trigger.enabled) {
                return Result<bool>::Success(true);
            }
        }

        return Result<bool>::Success(false);
    }

    Result<int32> GetTriggerCount(LogicalAxisId axis) const override {
        int32 count = 0;
        for (const auto& trigger : armed_triggers_) {
            if (trigger.axis == axis) {
                count += trigger.trigger_count;
            }
        }

        return Result<int32>::Success(count);
    }

    void setPendingArmContext(bool output_state, ReplayTriggerSpace space, std::string source) {
        pending_output_state_ = output_state;
        pending_space_ = space;
        pending_source_ = std::move(source);
    }

    void reset() {
        armed_triggers_.clear();
        last_config_ = TriggerConfig{};
        pending_output_state_ = true;
        pending_space_ = ReplayTriggerSpace::AxisPosition;
        pending_source_ = "compare_output";
    }

    void clearArmedTriggers(const TickInfo& tick, const std::string& reason) {
        if (!armed_triggers_.empty()) {
            recordEvent(tick, "TriggerController", reason);
        }
        armed_triggers_.clear();
    }

    void advance(const TickInfo& tick, double path_progress_mm) {
        for (auto& trigger : armed_triggers_) {
            if (!trigger.enabled || trigger.trigger_count > 0) {
                continue;
            }

            const auto observed = observedPosition(trigger, path_progress_mm);
            if (!observed.has_value() || *observed + 1e-6 < trigger.position_mm) {
                continue;
            }

            trigger.trigger_count += 1;
            trigger.last_trigger_position = *observed;
            recordEvent(
                tick,
                "TriggerController",
                "Triggered compare output " + trigger.signal_name + "=" + boolText(trigger.output_state) +
                    " at " + std::to_string(trigger.position_mm) + " mm.");

            if (trigger.pulse_width_us > 0U && trigger.output_state) {
                io_replay_->commandOutput(tick, trigger.signal_name, true, trigger.source);
                io_replay_->commandOutput(
                    tick,
                    trigger.signal_name,
                    false,
                    trigger.source,
                    durationFromMicroseconds(static_cast<std::int32_t>(trigger.pulse_width_us)));
            } else {
                io_replay_->commandOutput(tick, trigger.signal_name, trigger.output_state, trigger.source);
            }
        }
    }

    bool hasArmedTriggers() const {
        return std::any_of(
            armed_triggers_.begin(),
            armed_triggers_.end(),
            [](const ArmedTrigger& trigger) { return trigger.enabled && trigger.trigger_count == 0; });
    }

private:
    struct ArmedTrigger {
        ReplayTriggerSpace space{ReplayTriggerSpace::AxisPosition};
        LogicalAxisId axis{LogicalAxisId::INVALID};
        double position_mm{0.0};
        std::int32_t channel{0};
        std::string signal_name;
        bool output_state{false};
        std::uint32_t pulse_width_us{0};
        bool enabled{true};
        int32 trigger_count{0};
        double last_trigger_position{0.0};
        std::string source{"compare_output"};
    };

    std::optional<double> observedPosition(const ArmedTrigger& trigger, double path_progress_mm) const {
        if (trigger.space == ReplayTriggerSpace::PathProgress) {
            return path_progress_mm;
        }

        if (!motion_state_port_) {
            return std::nullopt;
        }

        const auto position = motion_state_port_->GetAxisPosition(trigger.axis);
        if (position.IsError()) {
            return std::nullopt;
        }

        return static_cast<double>(position.Value());
    }

    Result<std::string> signalNameForChannel(std::int32_t channel) const {
        const auto it = signal_by_channel_.find(channel);
        if (it == signal_by_channel_.end()) {
            return Result<std::string>::Failure(
                makeBridgeError(ErrorCode::INVALID_PARAMETER, "Trigger IO binding not found", "TriggerControllerAdapter"));
        }

        return Result<std::string>::Success(it->second);
    }

    void recordEvent(const TickInfo& tick, const std::string& source, const std::string& message) const {
        if (recorder_ != nullptr) {
            recorder_->recordEvent(tick, source, message);
        }
    }

    std::shared_ptr<IMotionStatePort> motion_state_port_;
    std::shared_ptr<DeterministicIoReplayController> io_replay_;
    Recorder*& recorder_;
    TickInfo& tick_;
    RuntimeBridgeBindings bindings_{};
    TriggerConfig last_config_{};
    bool pending_output_state_{true};
    ReplayTriggerSpace pending_space_{ReplayTriggerSpace::AxisPosition};
    std::string pending_source_{"compare_output"};
    std::vector<ArmedTrigger> armed_triggers_{};
    std::unordered_map<std::int32_t, std::string> signal_by_channel_{};
};

class ProcessRuntimeCoreRuntimeBridge final : public RuntimeBridge, public RuntimeBridgeControl {
public:
    explicit ProcessRuntimeCoreRuntimeBridge(RuntimeBridgeBindings bindings, DeterministicReplayPlan replay_plan)
        : bindings_(std::move(bindings)),
          replay_plan_(std::move(replay_plan)),
          event_publisher_(recorder_, last_tick_) {
        metadata_.mode = RuntimeBridgeMode::ProcessRuntimeCore;
        metadata_.process_runtime_core_linked = true;
        metadata_.next_integration_point =
            "SC-B-002 closed: scheme C is the canonical motion runtime path; compat engine remains baseline-only.";
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
    bool anyAxisMoving() const;
    ResultVoid armReplayTriggersForPath(const TickInfo& tick);
    void advanceReplay(const TickInfo& tick);
    void clearPendingReplay(const TickInfo& tick, const std::string& reason);
    void resetPathReplayTracking();
    std::optional<std::int16_t> runtimeChannelForSignal(const std::string& signal_name) const;
    void advanceActivePathCommand(CommandRecord& record, const TickInfo& tick);
    void startNextCommand(const TickInfo& tick);
    void pollActiveCommand(const TickInfo& tick);

    RuntimeBridgeMetadata metadata_{};
    RuntimeBridgeBindings bindings_{};
    DeterministicReplayPlan replay_plan_{};
    ProcessRuntimeCorePortAdapters runtime_ports_{};
    VirtualAxisGroup* axis_group_{nullptr};
    VirtualIo* io_{nullptr};
    Recorder* recorder_{nullptr};
    TickInfo last_tick_{};
    std::vector<LogicalAxisId> available_axes_{};
    std::unordered_map<std::string, std::int16_t> io_channels_by_signal_{};
    std::shared_ptr<DeterministicIoReplayController> io_replay_controller_{};
    std::shared_ptr<DeterministicTriggerControllerAdapter> trigger_controller_{};
    double active_path_progress_mm_{0.0};
    std::optional<Point2D> last_path_position_{};

    RecorderEventPublisher event_publisher_;
    std::shared_ptr<RuntimeConfigurationPort> configuration_port_{};

    std::unique_ptr<HomeAxesUseCase> home_use_case_{};
    std::unique_ptr<MoveToPositionUseCase> move_use_case_{};
    std::unique_ptr<MotionCoordinationUseCase> coordination_use_case_{};
    std::unique_ptr<MotionMonitoringUseCase> monitoring_use_case_{};
    std::unique_ptr<MotionSafetyUseCase> safety_use_case_{};
    std::unique_ptr<DeterministicPathExecutionUseCase> path_execution_use_case_{};

    std::deque<CommandRecord> commands_{};
    std::deque<std::uint64_t> pending_command_ids_{};
    std::optional<ActiveCommand> active_command_{};
    std::uint64_t next_command_id_{1};
    bool initialized_{false};
    bool stop_requested_{false};
    bool command_failure_{false};
};

std::string errorDetail(const Error& error) {
    if (!error.GetMessage().empty()) {
        return error.GetMessage();
    }
    return error.ToString();
}

bool isBenignCoordinateSystemStopError(const Error& error) {
    return error.GetCode() == ErrorCode::INVALID_PARAMETER &&
        error.GetMessage().find("Coordinate system not configured") != std::string::npos;
}

void ProcessRuntimeCoreRuntimeBridge::attach(
    VirtualAxisGroup& axis_group,
    VirtualIo& io,
    Recorder* recorder) {
    axis_group_ = &axis_group;
    io_ = &io;
    recorder_ = recorder;

    runtime_ports_ = createProcessRuntimeCorePortAdapters(axis_group, io, bindings_);
    bindings_ = runtime_ports_.resolved_bindings;

    available_axes_.clear();
    io_channels_by_signal_.clear();
    for (const auto& binding : bindings_.axis_bindings) {
        const auto axis = FromIndex(static_cast<std::int16_t>(binding.logical_axis_index));
        if (!IsValid(axis)) {
            continue;
        }
        if (std::find(available_axes_.begin(), available_axes_.end(), axis) == available_axes_.end()) {
            available_axes_.push_back(axis);
        }
    }
    for (const auto& binding : bindings_.io_bindings) {
        io_channels_by_signal_[binding.signal_name] = static_cast<std::int16_t>(binding.channel);
    }

    configuration_port_ = std::make_shared<RuntimeConfigurationPort>(available_axes_.size());
    io_replay_controller_ = std::make_shared<DeterministicIoReplayController>(io_, recorder_);
    io_replay_controller_->configure(replay_plan_);
    trigger_controller_ = std::make_shared<DeterministicTriggerControllerAdapter>(
        runtime_ports_.motion_runtime,
        io_replay_controller_,
        recorder_,
        last_tick_,
        bindings_);

    auto event_port = std::shared_ptr<IEventPublisherPort>(&event_publisher_, [](IEventPublisherPort*) {});
    MotionRuntimeAssemblyDependencies assembly_dependencies;
    assembly_dependencies.motion_runtime_port = runtime_ports_.motion_runtime;
    assembly_dependencies.interpolation_port = runtime_ports_.interpolation;
    assembly_dependencies.configuration_port = configuration_port_;
    assembly_dependencies.event_publisher_port = std::move(event_port);
    assembly_dependencies.trigger_controller_port = trigger_controller_;

    auto assembly = MotionRuntimeAssemblyFactory::Create(std::move(assembly_dependencies));
    home_use_case_ = std::move(assembly.home_use_case);
    move_use_case_ = std::move(assembly.move_use_case);
    coordination_use_case_ = std::move(assembly.coordination_use_case);
    monitoring_use_case_ = std::move(assembly.monitoring_use_case);
    safety_use_case_ = std::move(assembly.safety_use_case);
    path_execution_use_case_ = std::move(assembly.path_execution_use_case);
}

void ProcessRuntimeCoreRuntimeBridge::initialize(const TickInfo& tick) {
    initialized_ = true;
    stop_requested_ = false;
    command_failure_ = false;
    active_command_.reset();
    last_tick_ = tick;
    resetPathReplayTracking();
    if (trigger_controller_) {
        trigger_controller_->reset();
    }
    if (io_replay_controller_) {
        io_replay_controller_->configure(replay_plan_);
        io_replay_controller_->reset();
    }
    if (path_execution_use_case_) {
        (void)path_execution_use_case_->Cancel();
    }

    for (const auto axis : available_axes_) {
        if (!runtime_ports_.motion_runtime) {
            command_failure_ = true;
            recordEvent(tick, "RuntimeBridge", "Axis control port is not initialized.");
            break;
        }

        const auto enable_result = runtime_ports_.motion_runtime->EnableAxis(axis);
        if (enable_result.IsError()) {
            command_failure_ = true;
            recordEvent(
                tick,
                "RuntimeBridge",
                "Failed to enable logical axis " + std::to_string(static_cast<int>(ToIndex(axis))) + ": " +
                    errorDetail(enable_result.GetError()));
        }
    }

    recordEvent(
        tick,
        "RuntimeBridge",
        "Process-runtime-core bridge initialized on virtual clock and virtual devices.");
    if (!replay_plan_.io_delays.empty() || !replay_plan_.triggers.empty() || replay_plan_.valve.has_value()) {
        recordEvent(
            tick,
            "RuntimeBridge",
            "Initialized deterministic replay plan: " +
                std::to_string(replay_plan_.io_delays.size()) + " io delay specs, " +
                std::to_string(replay_plan_.triggers.size()) + " triggers, valve=" +
                (replay_plan_.valve.has_value() ? std::string("enabled") : std::string("disabled")) + ".");
    }
}

bool ProcessRuntimeCoreRuntimeBridge::advance(const TickInfo& tick) {
    last_tick_ = tick;

    if (!initialized_ || stop_requested_) {
        return false;
    }

    if (active_command_.has_value()) {
        auto stop_it = std::find_if(
            pending_command_ids_.begin(),
            pending_command_ids_.end(),
            [this](const std::uint64_t command_id) {
                const auto* record = findCommand(command_id);
                return record != nullptr &&
                    record->status.state == RuntimeBridgeCommandState::Pending &&
                    record->status.kind == RuntimeBridgeCommandKind::Stop;
            });

        if (stop_it != pending_command_ids_.end()) {
            const auto stop_command_id = *stop_it;
            pending_command_ids_.erase(stop_it);

            auto* stop_record = findCommand(stop_command_id);
            if (stop_record != nullptr) {
                markCommandStarted(*stop_record, tick, "Preempting active command on deterministic tick.");

                const auto& stop_command = std::get<RuntimeBridgeStopCommand>(stop_record->payload);
                if (stop_command.clear_pending_commands) {
                    cancelPendingCommands(tick, "Cancelled by stop command.");
                }
                clearPendingReplay(tick, "Cleared pending replay work after stop command.");

                if (active_command_->kind == RuntimeBridgeCommandKind::Home && home_use_case_) {
                    if (const auto* home = std::get_if<ActiveHomeCommand>(&active_command_->state)) {
                        if (home->axes.empty()) {
                            (void)home_use_case_->StopHoming();
                        } else {
                            (void)home_use_case_->StopHomingAxes(home->axes);
                        }
                    }
                }
                if (active_command_->kind == RuntimeBridgeCommandKind::ExecutePath && path_execution_use_case_) {
                    (void)path_execution_use_case_->Cancel();
                }

                if (auto* active_record = findCommand(active_command_->command_id)) {
                    markCommandCompleted(
                        *active_record,
                        RuntimeBridgeCommandState::Cancelled,
                        tick,
                        "Cancelled by stop command.");
                }
                active_command_.reset();

                if (coordination_use_case_) {
                    const auto stop_result = coordination_use_case_->StopCoordinateSystemMotion(1U);
                    if (stop_result.IsError() && !isBenignCoordinateSystemStopError(stop_result.GetError())) {
                        markCommandCompleted(
                            *stop_record,
                            RuntimeBridgeCommandState::Failed,
                            tick,
                            errorDetail(stop_result.GetError()));
                        cancelPendingCommands(tick, "Cancelled after active command failure.");
                        return false;
                    }
                }

                if (safety_use_case_) {
                    const auto stop_result = safety_use_case_->StopAllAxes(stop_command.immediate);
                    if (stop_result.IsError()) {
                        markCommandCompleted(
                            *stop_record,
                            RuntimeBridgeCommandState::Failed,
                            tick,
                            errorDetail(stop_result.GetError()));
                        cancelPendingCommands(tick, "Cancelled after active command failure.");
                        return false;
                    }
                }

                active_command_ = ActiveCommand{
                    stop_record->status.command_id,
                    RuntimeBridgeCommandKind::Stop,
                    ActiveStopCommand{stop_command.immediate}
                };
            }
        }
    }

    advanceReplay(tick);

    if (active_command_.has_value()) {
        pollActiveCommand(tick);
    }

    if (!active_command_.has_value() && !pending_command_ids_.empty()) {
        startNextCommand(tick);
    }

    return active_command_.has_value() ||
        !pending_command_ids_.empty() ||
        (trigger_controller_ && trigger_controller_->hasArmedTriggers()) ||
        (io_replay_controller_ && io_replay_controller_->hasPendingWork());
}

void ProcessRuntimeCoreRuntimeBridge::requestStop() {
    stop_requested_ = true;

    if (active_command_.has_value() && active_command_->kind == RuntimeBridgeCommandKind::Home && home_use_case_) {
        if (const auto* home = std::get_if<ActiveHomeCommand>(&active_command_->state)) {
            if (home->axes.empty()) {
                (void)home_use_case_->StopHoming();
            } else {
                (void)home_use_case_->StopHomingAxes(home->axes);
            }
        }
    }
    if (active_command_.has_value() &&
        active_command_->kind == RuntimeBridgeCommandKind::ExecutePath &&
        path_execution_use_case_) {
        (void)path_execution_use_case_->Cancel();
    }

    cancelPendingCommands(last_tick_, "Cancelled by session stop.");

    if (active_command_.has_value()) {
        if (auto* record = findCommand(active_command_->command_id)) {
            markCommandCompleted(
                *record,
                RuntimeBridgeCommandState::Cancelled,
                last_tick_,
                "Cancelled by session stop.");
        }
        active_command_.reset();
    }

    if (coordination_use_case_) {
        (void)coordination_use_case_->StopCoordinateSystemMotion(1U);
    }
    if (safety_use_case_) {
        (void)safety_use_case_->StopAllAxes(true);
    }
    clearPendingReplay(last_tick_, "Cleared pending replay work after session stop.");
}

std::uint64_t ProcessRuntimeCoreRuntimeBridge::submitHome(const RuntimeBridgeHomeCommand& command) {
    return enqueueCommand(RuntimeBridgeCommandKind::Home, command);
}

std::uint64_t ProcessRuntimeCoreRuntimeBridge::submitMove(const RuntimeBridgeMoveCommand& command) {
    return enqueueCommand(RuntimeBridgeCommandKind::Move, command);
}

std::uint64_t ProcessRuntimeCoreRuntimeBridge::submitPath(const RuntimeBridgePathCommand& command) {
    return enqueueCommand(RuntimeBridgeCommandKind::ExecutePath, command);
}

std::uint64_t ProcessRuntimeCoreRuntimeBridge::submitStop(const RuntimeBridgeStopCommand& command) {
    return enqueueCommand(RuntimeBridgeCommandKind::Stop, command);
}

std::optional<RuntimeBridgeCommandStatus> ProcessRuntimeCoreRuntimeBridge::commandStatus(
    std::uint64_t command_id) const {
    const auto* record = findCommand(command_id);
    if (record == nullptr) {
        return std::nullopt;
    }

    return record->status;
}

std::vector<RuntimeBridgeCommandStatus> ProcessRuntimeCoreRuntimeBridge::commandHistory() const {
    std::vector<RuntimeBridgeCommandStatus> history;
    history.reserve(commands_.size());
    for (const auto& record : commands_) {
        history.push_back(record.status);
    }
    return history;
}

bool ProcessRuntimeCoreRuntimeBridge::hasPendingCommands() const {
    return active_command_.has_value() ||
        !pending_command_ids_.empty() ||
        (trigger_controller_ && trigger_controller_->hasArmedTriggers()) ||
        (io_replay_controller_ && io_replay_controller_->hasPendingWork());
}

bool ProcessRuntimeCoreRuntimeBridge::hasCommandFailure() const {
    return command_failure_;
}

template <typename TCommand>
std::uint64_t ProcessRuntimeCoreRuntimeBridge::enqueueCommand(
    RuntimeBridgeCommandKind kind,
    const TCommand& command) {
    CommandRecord record;
    record.status.command_id = next_command_id_++;
    record.status.kind = kind;
    record.status.state = RuntimeBridgeCommandState::Pending;
    record.status.submitted_tick = last_tick_.tick_index;
    record.status.detail = "Queued " + std::string(commandKindName(kind)) + " command.";
    record.payload = command;

    const auto command_id = record.status.command_id;
    commands_.push_back(record);
    pending_command_ids_.push_back(command_id);

    recordEvent(
        last_tick_,
        "RuntimeBridge",
        "Queued " + std::string(commandKindName(kind)) + " command #" + std::to_string(command_id) + ".");
    return command_id;
}

CommandRecord* ProcessRuntimeCoreRuntimeBridge::findCommand(std::uint64_t command_id) {
    auto it = std::find_if(
        commands_.begin(),
        commands_.end(),
        [command_id](const CommandRecord& record) { return record.status.command_id == command_id; });
    return it == commands_.end() ? nullptr : &(*it);
}

const CommandRecord* ProcessRuntimeCoreRuntimeBridge::findCommand(std::uint64_t command_id) const {
    auto it = std::find_if(
        commands_.begin(),
        commands_.end(),
        [command_id](const CommandRecord& record) { return record.status.command_id == command_id; });
    return it == commands_.end() ? nullptr : &(*it);
}

void ProcessRuntimeCoreRuntimeBridge::recordEvent(
    const TickInfo& tick,
    const std::string& source,
    const std::string& message) {
    if (recorder_ != nullptr) {
        recorder_->recordEvent(tick, source, message);
    }
}

void ProcessRuntimeCoreRuntimeBridge::markCommandStarted(
    CommandRecord& record,
    const TickInfo& tick,
    const std::string& detail) {
    record.status.state = RuntimeBridgeCommandState::Active;
    record.status.started_tick = tick.tick_index;
    record.status.detail = detail;

    recordEvent(
        tick,
        "RuntimeBridge",
        "Started " + std::string(commandKindName(record.status.kind)) + " command #" +
            std::to_string(record.status.command_id) + ": " + detail);
}

void ProcessRuntimeCoreRuntimeBridge::markCommandCompleted(
    CommandRecord& record,
    RuntimeBridgeCommandState state,
    const TickInfo& tick,
    const std::string& detail) {
    record.status.state = state;
    record.status.completed_tick = tick.tick_index;
    record.status.detail = detail;

    if (state == RuntimeBridgeCommandState::Failed) {
        command_failure_ = true;
    }

    std::string state_name = "completed";
    if (state == RuntimeBridgeCommandState::Failed) {
        state_name = "failed";
    } else if (state == RuntimeBridgeCommandState::Cancelled) {
        state_name = "cancelled";
    }

    recordEvent(
        tick,
        "RuntimeBridge",
        std::string(commandKindName(record.status.kind)) + " command #" +
            std::to_string(record.status.command_id) + " " + state_name + ": " + detail);
}

void ProcessRuntimeCoreRuntimeBridge::cancelPendingCommands(
    const TickInfo& tick,
    const std::string& detail) {
    for (const auto command_id : pending_command_ids_) {
        auto* record = findCommand(command_id);
        if (record == nullptr || record->status.state != RuntimeBridgeCommandState::Pending) {
            continue;
        }

        record->status.state = RuntimeBridgeCommandState::Cancelled;
        record->status.completed_tick = tick.tick_index;
        record->status.detail = detail;

        recordEvent(
            tick,
            "RuntimeBridge",
            std::string(commandKindName(record->status.kind)) + " command #" +
                std::to_string(record->status.command_id) + " cancelled: " + detail);
    }

    pending_command_ids_.clear();
}

void ProcessRuntimeCoreRuntimeBridge::failActiveCommand(
    const TickInfo& tick,
    const std::string& detail) {
    if (active_command_.has_value()) {
        if (active_command_->kind == RuntimeBridgeCommandKind::ExecutePath && path_execution_use_case_) {
            (void)path_execution_use_case_->Cancel();
        }
        if (auto* record = findCommand(active_command_->command_id)) {
            markCommandCompleted(*record, RuntimeBridgeCommandState::Failed, tick, detail);
        } else {
            command_failure_ = true;
        }
        active_command_.reset();
    } else {
        command_failure_ = true;
    }

    cancelPendingCommands(tick, "Cancelled after active command failure.");
    clearPendingReplay(tick, "Cleared pending replay work after active command failure.");
}

std::vector<LogicalAxisId> ProcessRuntimeCoreRuntimeBridge::axesForHomeCommand(
    const RuntimeBridgeHomeCommand& command) const {
    if (command.home_all_axes || command.logical_axis_indices.empty()) {
        return available_axes_;
    }

    std::vector<LogicalAxisId> axes;
    for (const auto logical_axis_index : command.logical_axis_indices) {
        const auto axis = FromIndex(static_cast<std::int16_t>(logical_axis_index));
        if (!IsValid(axis)) {
            continue;
        }
        if (std::find(available_axes_.begin(), available_axes_.end(), axis) == available_axes_.end()) {
            continue;
        }
        if (std::find(axes.begin(), axes.end(), axis) == axes.end()) {
            axes.push_back(axis);
        }
    }

    return axes;
}

bool ProcessRuntimeCoreRuntimeBridge::axesHomed(const std::vector<LogicalAxisId>& axes) const {
    if (!runtime_ports_.motion_runtime) {
        return false;
    }

    for (const auto axis : axes) {
        const auto homed_result = runtime_ports_.motion_runtime->IsAxisHomed(axis);
        if (homed_result.IsError() || !homed_result.Value()) {
            return false;
        }
    }

    return true;
}

bool ProcessRuntimeCoreRuntimeBridge::moveReachedTarget(const ActiveMoveCommand& move) const {
    if (!runtime_ports_.motion_runtime) {
        return false;
    }

    const auto position_result = runtime_ports_.motion_runtime->GetCurrentPosition();
    if (position_result.IsError()) {
        return false;
    }

    const auto status_result = runtime_ports_.motion_runtime->GetAllAxesStatus();
    if (status_result.IsError()) {
        return false;
    }

    for (const auto& status : status_result.Value()) {
        if (status.state == Siligen::Domain::Motion::Ports::MotionState::MOVING ||
            status.state == Siligen::Domain::Motion::Ports::MotionState::HOMING) {
            return false;
        }
    }

    const auto position = position_result.Value();
    const bool x_reached =
        std::find(available_axes_.begin(), available_axes_.end(), LogicalAxisId::X) == available_axes_.end() ||
        almostEqual(position.x, static_cast<float>(move.target_x_mm), 0.01);
    const bool y_reached =
        std::find(available_axes_.begin(), available_axes_.end(), LogicalAxisId::Y) == available_axes_.end() ||
        almostEqual(position.y, static_cast<float>(move.target_y_mm), 0.01);
    return x_reached && y_reached;
}

bool ProcessRuntimeCoreRuntimeBridge::anyAxisMoving() const {
    if (!runtime_ports_.motion_runtime) {
        return false;
    }

    const auto statuses = runtime_ports_.motion_runtime->GetAllAxesStatus();
    if (statuses.IsError()) {
        return false;
    }

    for (const auto& status : statuses.Value()) {
        if (status.state == Siligen::Domain::Motion::Ports::MotionState::MOVING ||
            status.state == Siligen::Domain::Motion::Ports::MotionState::HOMING) {
            return true;
        }
    }

    return false;
}

ResultVoid ProcessRuntimeCoreRuntimeBridge::armReplayTriggersForPath(const TickInfo& tick) {
    if (!coordination_use_case_ || !trigger_controller_ || replay_plan_.triggers.empty()) {
        return ResultVoid::Success();
    }

    trigger_controller_->clearArmedTriggers(tick, "Cleared previously armed compare outputs before replay re-arm.");

    auto ordered_triggers = replay_plan_.triggers;
    std::stable_sort(
        ordered_triggers.begin(),
        ordered_triggers.end(),
        [](const ReplayTriggerSpec& lhs, const ReplayTriggerSpec& rhs) {
            return lhs.position_mm < rhs.position_mm;
        });

    for (const auto& trigger : ordered_triggers) {
        const auto channel = runtimeChannelForSignal(trigger.channel);
        if (!channel.has_value()) {
            return ResultVoid::Failure(
                makeBridgeError(
                    ErrorCode::INVALID_PARAMETER,
                    "Replay trigger channel binding not found",
                    "ProcessRuntimeCoreRuntimeBridge"));
        }

        trigger_controller_->setPendingArmContext(trigger.state, trigger.space, "canonical_trigger");

        MotionIOCommand command;
        command.channel = *channel;
        command.type = MotionIOCommand::Type::COMPARE_OUTPUT;
        command.value = trigger.state;
        command.position = static_cast<float32>(trigger.position_mm);
        command.pulse_width_us = trigger.pulse_width_us;

        const auto result = coordination_use_case_->ConfigureCompareOutput(command);
        if (result.IsError()) {
            return result;
        }
    }

    recordEvent(
        tick,
        "RuntimeBridge",
        "Armed " + std::to_string(ordered_triggers.size()) + " canonical compare triggers for deterministic replay.");
    return ResultVoid::Success();
}

void ProcessRuntimeCoreRuntimeBridge::advanceReplay(const TickInfo& tick) {
    if (!io_replay_controller_ && !trigger_controller_) {
        return;
    }

    if (active_command_.has_value() &&
        active_command_->kind == RuntimeBridgeCommandKind::ExecutePath &&
        runtime_ports_.motion_runtime) {
        const auto position = runtime_ports_.motion_runtime->GetCurrentPosition();
        if (position.IsSuccess()) {
            if (last_path_position_.has_value()) {
                active_path_progress_mm_ += static_cast<double>(last_path_position_->DistanceTo(position.Value()));
            }
            last_path_position_ = position.Value();
        }
    }

    if (trigger_controller_) {
        trigger_controller_->advance(tick, active_path_progress_mm_);
    }
    if (io_replay_controller_) {
        io_replay_controller_->advance(tick);
    }
}

void ProcessRuntimeCoreRuntimeBridge::clearPendingReplay(const TickInfo& tick, const std::string& reason) {
    if (trigger_controller_) {
        trigger_controller_->clearArmedTriggers(tick, reason);
    }
    if (io_replay_controller_) {
        io_replay_controller_->clearPending(tick, reason);
    }
    resetPathReplayTracking();
}

void ProcessRuntimeCoreRuntimeBridge::resetPathReplayTracking() {
    active_path_progress_mm_ = 0.0;
    last_path_position_.reset();

    if (!runtime_ports_.motion_runtime) {
        return;
    }

    const auto position = runtime_ports_.motion_runtime->GetCurrentPosition();
    if (position.IsSuccess()) {
        last_path_position_ = position.Value();
    }
}

std::optional<std::int16_t> ProcessRuntimeCoreRuntimeBridge::runtimeChannelForSignal(
    const std::string& signal_name) const {
    const auto it = io_channels_by_signal_.find(signal_name);
    if (it == io_channels_by_signal_.end()) {
        return std::nullopt;
    }

    return it->second;
}

void ProcessRuntimeCoreRuntimeBridge::advanceActivePathCommand(
    CommandRecord& record,
    const TickInfo& tick) {
    if (!path_execution_use_case_) {
        failActiveCommand(tick, "Deterministic path execution use case is not initialized.");
        return;
    }

    const auto advance_result = path_execution_use_case_->Advance();
    if (advance_result.IsError()) {
        failActiveCommand(tick, errorDetail(advance_result.GetError()));
        return;
    }

    const auto status = advance_result.Value();
    if (status.state == DeterministicPathExecutionState::FAILED) {
        failActiveCommand(tick, status.detail);
        return;
    }
    if (status.state == DeterministicPathExecutionState::COMPLETED) {
        markCommandCompleted(record, RuntimeBridgeCommandState::Completed, tick, status.detail);
        if (trigger_controller_) {
            trigger_controller_->clearArmedTriggers(tick, "Cleared remaining replay compare outputs after path completion.");
        }
        resetPathReplayTracking();
        active_command_.reset();
        return;
    }
    if (status.state == DeterministicPathExecutionState::CANCELLED) {
        markCommandCompleted(record, RuntimeBridgeCommandState::Cancelled, tick, status.detail);
        if (trigger_controller_) {
            trigger_controller_->clearArmedTriggers(tick, "Cleared replay compare outputs after path cancellation.");
        }
        resetPathReplayTracking();
        active_command_.reset();
    }
}

void ProcessRuntimeCoreRuntimeBridge::startNextCommand(const TickInfo& tick) {
    if (pending_command_ids_.empty()) {
        return;
    }

    const auto command_id = pending_command_ids_.front();
    pending_command_ids_.pop_front();

    auto* record = findCommand(command_id);
    if (record == nullptr || record->status.state != RuntimeBridgeCommandState::Pending) {
        return;
    }

    markCommandStarted(*record, tick, "Accepted by deterministic runtime bridge.");

    switch (record->status.kind) {
    case RuntimeBridgeCommandKind::Home: {
        if (!home_use_case_) {
            markCommandCompleted(
                *record,
                RuntimeBridgeCommandState::Failed,
                tick,
                "Home use case is not initialized.");
            cancelPendingCommands(tick, "Cancelled after active command failure.");
            return;
        }

        const auto& command = std::get<RuntimeBridgeHomeCommand>(record->payload);
        const auto axes = axesForHomeCommand(command);
        if (axes.empty()) {
            markCommandCompleted(
                *record,
                RuntimeBridgeCommandState::Failed,
                tick,
                "Home command does not resolve to any available logical axis.");
            cancelPendingCommands(tick, "Cancelled after active command failure.");
            return;
        }

        HomeAxesRequest request;
        request.home_all_axes = command.home_all_axes || command.logical_axis_indices.empty();
        request.force_rehome = command.force_rehome;
        request.wait_for_completion = false;
        request.timeout_ms = command.timeout_ms;
        if (!request.home_all_axes) {
            request.axes = axes;
        }

        const auto result = home_use_case_->Execute(request);
        if (result.IsError()) {
            markCommandCompleted(
                *record,
                RuntimeBridgeCommandState::Failed,
                tick,
                errorDetail(result.GetError()));
            cancelPendingCommands(tick, "Cancelled after active command failure.");
            return;
        }

        active_command_ = ActiveCommand{
            record->status.command_id,
            RuntimeBridgeCommandKind::Home,
            ActiveHomeCommand{request.home_all_axes ? available_axes_ : axes}
        };
        return;
    }

    case RuntimeBridgeCommandKind::Move: {
        if (!move_use_case_) {
            markCommandCompleted(
                *record,
                RuntimeBridgeCommandState::Failed,
                tick,
                "Move use case is not initialized.");
            cancelPendingCommands(tick, "Cancelled after active command failure.");
            return;
        }

        const auto& command = std::get<RuntimeBridgeMoveCommand>(record->payload);

        MoveToPositionRequest request;
        request.target_position = Point2D(
            static_cast<float>(command.target_x_mm),
            static_cast<float>(command.target_y_mm));
        request.movement_speed = static_cast<float>(command.velocity_mm_per_s);
        request.validate_state = false;
        request.wait_for_completion = false;
        request.timeout_ms = 1;

        const auto result = move_use_case_->Execute(request);
        if (result.IsError()) {
            markCommandCompleted(
                *record,
                RuntimeBridgeCommandState::Failed,
                tick,
                errorDetail(result.GetError()));
            cancelPendingCommands(tick, "Cancelled after active command failure.");
            return;
        }

        active_command_ = ActiveCommand{
            record->status.command_id,
            RuntimeBridgeCommandKind::Move,
            ActiveMoveCommand{command.target_x_mm, command.target_y_mm}
        };
        return;
    }

    case RuntimeBridgeCommandKind::ExecutePath: {
        if (!path_execution_use_case_) {
            markCommandCompleted(
                *record,
                RuntimeBridgeCommandState::Failed,
                tick,
                "Deterministic path execution use case is not initialized.");
            cancelPendingCommands(tick, "Cancelled after active command failure.");
            return;
        }

        const auto& command = std::get<RuntimeBridgePathCommand>(record->payload);
        DeterministicPathExecutionRequest request;
        request.max_velocity_mm_s = static_cast<float>(command.max_velocity_mm_per_s);
        request.max_acceleration_mm_s2 = static_cast<float>(command.max_acceleration_mm_per_s2);
        request.sample_dt_s = static_cast<float>(command.sample_dt_s);
        request.axis_map = {LogicalAxisId::X, LogicalAxisId::Y};
        request.segments.reserve(command.segments.size());
        for (const auto& segment : command.segments) {
            DeterministicPathSegment path_segment;
            path_segment.end_point = Point2D(
                static_cast<float>(segment.end_x_mm),
                static_cast<float>(segment.end_y_mm));
            path_segment.center_point = Point2D(
                static_cast<float>(segment.center_x_mm),
                static_cast<float>(segment.center_y_mm));
            if (segment.type == RuntimePathSegmentType::Line) {
                path_segment.type = DeterministicPathSegmentType::LINE;
            } else if (segment.type == RuntimePathSegmentType::ArcClockwise) {
                path_segment.type = DeterministicPathSegmentType::ARC_CW;
            } else {
                path_segment.type = DeterministicPathSegmentType::ARC_CCW;
            }
            request.segments.push_back(path_segment);
        }

        const auto start_result = path_execution_use_case_->Start(request);
        if (start_result.IsError()) {
            markCommandCompleted(
                *record,
                RuntimeBridgeCommandState::Failed,
                tick,
                errorDetail(start_result.GetError()));
            cancelPendingCommands(tick, "Cancelled after active command failure.");
            return;
        }

        recordEvent(
            tick,
            "RuntimeBridge",
            "Delegated path command #" + std::to_string(record->status.command_id) +
                " to process-runtime-core deterministic path execution.");

        resetPathReplayTracking();
        const auto replay_result = armReplayTriggersForPath(tick);
        if (replay_result.IsError()) {
            markCommandCompleted(
                *record,
                RuntimeBridgeCommandState::Failed,
                tick,
                errorDetail(replay_result.GetError()));
            cancelPendingCommands(tick, "Cancelled after active command failure.");
            return;
        }

        active_command_ = ActiveCommand{
            record->status.command_id,
            RuntimeBridgeCommandKind::ExecutePath,
            ActivePathCommand{}
        };

        advanceActivePathCommand(*record, tick);
        return;
    }

    case RuntimeBridgeCommandKind::Stop: {
        const auto& command = std::get<RuntimeBridgeStopCommand>(record->payload);
        if (command.clear_pending_commands) {
            cancelPendingCommands(tick, "Cancelled by stop command.");
        }
        clearPendingReplay(tick, "Cleared pending replay work after stop command.");

        if (coordination_use_case_) {
            const auto stop_result = coordination_use_case_->StopCoordinateSystemMotion(1U);
            if (stop_result.IsError() && !isBenignCoordinateSystemStopError(stop_result.GetError())) {
                markCommandCompleted(
                    *record,
                    RuntimeBridgeCommandState::Failed,
                    tick,
                    errorDetail(stop_result.GetError()));
                cancelPendingCommands(tick, "Cancelled after active command failure.");
                return;
            }
        }

        if (safety_use_case_) {
            const auto stop_result = safety_use_case_->StopAllAxes(command.immediate);
            if (stop_result.IsError()) {
                markCommandCompleted(
                    *record,
                    RuntimeBridgeCommandState::Failed,
                    tick,
                    errorDetail(stop_result.GetError()));
                cancelPendingCommands(tick, "Cancelled after active command failure.");
                return;
            }
        }

        active_command_ = ActiveCommand{
            record->status.command_id,
            RuntimeBridgeCommandKind::Stop,
            ActiveStopCommand{command.immediate}
        };
        return;
    }
    }
}

void ProcessRuntimeCoreRuntimeBridge::pollActiveCommand(const TickInfo& tick) {
    if (!active_command_.has_value()) {
        return;
    }

    if (runtime_ports_.motion_runtime) {
        const auto statuses = runtime_ports_.motion_runtime->GetAllAxesStatus();
        if (statuses.IsError()) {
            failActiveCommand(tick, errorDetail(statuses.GetError()));
            return;
        }

        for (const auto& status : statuses.Value()) {
            if (status.has_error) {
                failActiveCommand(
                    tick,
                    "Virtual axis reported error code " + std::to_string(status.error_code) + ".");
                return;
            }
        }
    }

    auto* record = findCommand(active_command_->command_id);
    if (record == nullptr) {
        command_failure_ = true;
        active_command_.reset();
        return;
    }

    switch (active_command_->kind) {
    case RuntimeBridgeCommandKind::Home: {
        const auto* home = std::get_if<ActiveHomeCommand>(&active_command_->state);
        if (home == nullptr) {
            failActiveCommand(tick, "Active home command state is invalid.");
            return;
        }

        if (axesHomed(home->axes)) {
            markCommandCompleted(*record, RuntimeBridgeCommandState::Completed, tick, "Home command completed.");
            active_command_.reset();
            return;
        }

        if (!anyAxisMoving()) {
            failActiveCommand(tick, "Home command stopped before all requested axes were homed.");
        }
        return;
    }

    case RuntimeBridgeCommandKind::Move: {
        const auto* move = std::get_if<ActiveMoveCommand>(&active_command_->state);
        if (move == nullptr) {
            failActiveCommand(tick, "Active move command state is invalid.");
            return;
        }

        if (moveReachedTarget(*move)) {
            markCommandCompleted(*record, RuntimeBridgeCommandState::Completed, tick, "Move command completed.");
            active_command_.reset();
            return;
        }

        if (!anyAxisMoving()) {
            failActiveCommand(tick, "Move command stopped before reaching its target position.");
        }
        return;
    }

    case RuntimeBridgeCommandKind::ExecutePath: {
        const auto* path = std::get_if<ActivePathCommand>(&active_command_->state);
        if (path == nullptr) {
            failActiveCommand(tick, "Active path command state is invalid.");
            return;
        }

        advanceActivePathCommand(*record, tick);
        return;
    }

    case RuntimeBridgeCommandKind::Stop: {
        if (!anyAxisMoving()) {
            markCommandCompleted(*record, RuntimeBridgeCommandState::Completed, tick, "Stop command completed.");
            active_command_.reset();
        }
        return;
    }
    }
}

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
    const RuntimeBridgeBindings& bindings,
    const DeterministicReplayPlan& replay_plan) {
#if defined(SIM_ENGINE_SCHEME_C_HAS_PROCESS_RUNTIME_CORE)
    return std::make_unique<ProcessRuntimeCoreRuntimeBridge>(bindings, replay_plan);
#else
    return createRuntimeBridgeSeam(bindings);
#endif
}

std::unique_ptr<RuntimeBridge> createRuntimeBridgeSeam(
    const RuntimeBridgeBindings& bindings) {
    return std::make_unique<RuntimeBridgeSeam>(bindings);
}

}  // namespace sim::scheme_c
