#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "application/usecases/motion/coordination/MotionCoordinationUseCase.h"
#include "application/usecases/motion/ptp/MoveToPositionUseCase.h"
#include "application/usecases/motion/runtime/MotionRuntimeAssemblyFactory.h"
#include "domain/configuration/ports/IConfigurationPort.h"
#include "domain/dispensing/ports/ITriggerControllerPort.h"
#include "domain/motion/ports/IInterpolationPort.h"
#include "domain/motion/ports/IMotionRuntimePort.h"
#include "domain/system/ports/IEventPublisherPort.h"
#include "shared/types/Error.h"

namespace {

using MotionRuntimeAssemblyDependencies =
    Siligen::Application::UseCases::Motion::Runtime::MotionRuntimeAssemblyDependencies;
using MotionRuntimeAssemblyFactory = Siligen::Application::UseCases::Motion::Runtime::MotionRuntimeAssemblyFactory;
using MotionIOCommand = Siligen::Application::UseCases::Motion::Coordination::MotionIOCommand;
using MoveToPositionRequest = Siligen::Application::UseCases::Motion::PTP::MoveToPositionRequest;
using DeterministicPathExecutionRequest =
    Siligen::Application::UseCases::Motion::Trajectory::DeterministicPathExecutionRequest;
using DeterministicPathExecutionState =
    Siligen::Application::UseCases::Motion::Trajectory::DeterministicPathExecutionState;
using DeterministicPathSegment = Siligen::Application::UseCases::Motion::Trajectory::DeterministicPathSegment;
using DeterministicPathSegmentType =
    Siligen::Application::UseCases::Motion::Trajectory::DeterministicPathSegmentType;
using IConfigurationPort = Siligen::Domain::Configuration::Ports::IConfigurationPort;
using HomingConfig = Siligen::Domain::Configuration::Ports::HomingConfig;
using TriggerConfig = Siligen::Domain::Dispensing::Ports::TriggerConfig;
using TriggerStatus = Siligen::Domain::Dispensing::Ports::TriggerStatus;
using CoordinateSystemConfig = Siligen::Domain::Motion::Ports::CoordinateSystemConfig;
using CoordinateSystemStatus = Siligen::Domain::Motion::Ports::CoordinateSystemStatus;
using CoordinateSystemState = Siligen::Domain::Motion::Ports::CoordinateSystemState;
using IMotionRuntimePort = Siligen::Domain::Motion::Ports::IMotionRuntimePort;
using IInterpolationPort = Siligen::Domain::Motion::Ports::IInterpolationPort;
using InterpolationData = Siligen::Domain::Motion::Ports::InterpolationData;
using JogParameters = Siligen::Domain::Motion::Ports::JogParameters;
using AxisConfiguration = Siligen::Domain::Motion::Ports::AxisConfiguration;
using IOStatus = Siligen::Domain::Motion::Ports::IOStatus;
using HomingStatus = Siligen::Domain::Motion::Ports::HomingStatus;
using HomingState = Siligen::Domain::Motion::Ports::HomingState;
using MotionCommand = Siligen::Domain::Motion::Ports::MotionCommand;
using MotionState = Siligen::Domain::Motion::Ports::MotionState;
using MotionStatus = Siligen::Domain::Motion::Ports::MotionStatus;
using DomainEvent = Siligen::Domain::System::Ports::DomainEvent;
using EventType = Siligen::Domain::System::Ports::EventType;
using EventHandler = Siligen::Domain::System::Ports::EventHandler;
template <typename T>
using Result = Siligen::Shared::Types::Result<T>;
using ResultVoid = Siligen::Shared::Types::Result<void>;
using Error = Siligen::Shared::Types::Error;
using ErrorCode = Siligen::Shared::Types::ErrorCode;
using LogicalAxisId = Siligen::Shared::Types::LogicalAxisId;
using Point2D = Siligen::Shared::Types::Point2D;
using float32 = Siligen::Shared::Types::float32;
using int16 = std::int16_t;
using int32 = std::int32_t;
using uint32 = std::uint32_t;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename T>
Result<T> notImplemented(const char* method) {
    return Result<T>::Failure(Error(ErrorCode::NOT_IMPLEMENTED, method, "MotionRuntimeAssemblyFactoryTest"));
}

ResultVoid notImplementedVoid(const char* method) {
    return ResultVoid::Failure(Error(ErrorCode::NOT_IMPLEMENTED, method, "MotionRuntimeAssemblyFactoryTest"));
}

class FakeInterpolationPort final : public IInterpolationPort {
public:
    ResultVoid ConfigureCoordinateSystem(int16 coord_sys, const CoordinateSystemConfig& config) override {
        ++configure_calls;
        last_coord_sys = coord_sys;
        last_config = config;
        return ResultVoid::Success();
    }
    ResultVoid AddInterpolationData(int16 coord_sys, const InterpolationData& data) override {
        ++add_calls;
        last_add_coord_sys = coord_sys;
        staged.push_back(data);
        segments.push_back(data);
        return ResultVoid::Success();
    }
    ResultVoid ClearInterpolationBuffer(int16 coord_sys) override {
        ++clear_calls;
        last_clear_coord_sys = coord_sys;
        staged.clear();
        queued.clear();
        active = false;
        return ResultVoid::Success();
    }
    ResultVoid FlushInterpolationData(int16 coord_sys) override {
        ++flush_calls;
        last_flush_coord_sys = coord_sys;
        queued.insert(queued.end(), staged.begin(), staged.end());
        staged.clear();
        return ResultVoid::Success();
    }
    ResultVoid StartCoordinateSystemMotion(uint32 coord_sys_mask) override {
        ++start_calls;
        last_start_mask = coord_sys_mask;
        if (!active && !queued.empty()) {
            active = true;
            queued.erase(queued.begin());
        }
        return ResultVoid::Success();
    }
    ResultVoid StopCoordinateSystemMotion(uint32 coord_sys_mask) override {
        ++stop_calls;
        last_stop_mask = coord_sys_mask;
        staged.clear();
        queued.clear();
        active = false;
        return ResultVoid::Success();
    }
    ResultVoid SetCoordinateSystemVelocityOverride(int16, float32) override { return notImplementedVoid("SetCoordinateSystemVelocityOverride"); }
    ResultVoid EnableCoordinateSystemSCurve(int16, float32) override { return notImplementedVoid("EnableCoordinateSystemSCurve"); }
    ResultVoid DisableCoordinateSystemSCurve(int16) override { return notImplementedVoid("DisableCoordinateSystemSCurve"); }
    ResultVoid SetConstLinearVelocityMode(int16, bool, uint32) override { return notImplementedVoid("SetConstLinearVelocityMode"); }
    Result<uint32> GetInterpolationBufferSpace(int16) const override {
        const auto occupancy = staged.size() + queued.size() + (active ? 1U : 0U);
        return Result<uint32>::Success(buffer_capacity > occupancy ? static_cast<uint32>(buffer_capacity - occupancy) : 0U);
    }
    Result<uint32> GetLookAheadBufferSpace(int16) const override {
        return Result<uint32>::Success(lookahead_capacity > queued.size() ? static_cast<uint32>(lookahead_capacity - queued.size()) : 0U);
    }
    Result<CoordinateSystemStatus> GetCoordinateSystemStatus(int16) const override {
        CoordinateSystemStatus status;
        status.is_moving = active;
        status.state = active ? CoordinateSystemState::MOVING : CoordinateSystemState::IDLE;
        status.remaining_segments = static_cast<uint32>(queued.size() + (active ? 1U : 0U));
        status.raw_segment = static_cast<int32>(status.remaining_segments);
        return Result<CoordinateSystemStatus>::Success(status);
    }

    int configure_calls = 0;
    int add_calls = 0;
    int clear_calls = 0;
    int flush_calls = 0;
    int start_calls = 0;
    int stop_calls = 0;
    int16 last_coord_sys = 0;
    int16 last_add_coord_sys = 0;
    int16 last_clear_coord_sys = 0;
    int16 last_flush_coord_sys = 0;
    uint32 last_start_mask = 0;
    uint32 last_stop_mask = 0;
    CoordinateSystemConfig last_config{};
    std::vector<InterpolationData> segments{};
    std::vector<InterpolationData> staged{};
    std::vector<InterpolationData> queued{};
    std::size_t buffer_capacity = 8U;
    std::size_t lookahead_capacity = 4U;
    bool active = false;
};

class FakeMotionRuntimePort final : public IMotionRuntimePort {
public:
    ResultVoid Connect(const std::string&, const std::string&, int16) override { connected = true; return ResultVoid::Success(); }
    ResultVoid Disconnect() override { connected = false; return ResultVoid::Success(); }
    Result<bool> IsConnected() const override { return Result<bool>::Success(connected); }
    ResultVoid Reset() override { connected = true; return ResultVoid::Success(); }
    Result<std::string> GetCardInfo() const override { return Result<std::string>::Success("fake-motion-runtime"); }

    ResultVoid EnableAxis(LogicalAxisId) override { return ResultVoid::Success(); }
    ResultVoid DisableAxis(LogicalAxisId) override { return ResultVoid::Success(); }
    Result<bool> IsAxisEnabled(LogicalAxisId) const override { return Result<bool>::Success(true); }
    ResultVoid ClearAxisStatus(LogicalAxisId) override { return ResultVoid::Success(); }
    ResultVoid ClearPosition(LogicalAxisId) override { return ResultVoid::Success(); }
    ResultVoid SetAxisVelocity(LogicalAxisId, float32) override { return ResultVoid::Success(); }
    ResultVoid SetAxisAcceleration(LogicalAxisId, float32) override { return ResultVoid::Success(); }
    ResultVoid SetSoftLimits(LogicalAxisId, float32, float32) override { return ResultVoid::Success(); }
    ResultVoid ConfigureAxis(LogicalAxisId, const AxisConfiguration&) override { return ResultVoid::Success(); }
    ResultVoid SetHardLimits(LogicalAxisId, short, short, short, short) override { return ResultVoid::Success(); }
    ResultVoid EnableHardLimits(LogicalAxisId, bool, short) override { return ResultVoid::Success(); }
    ResultVoid SetHardLimitPolarity(LogicalAxisId, short, short) override { return ResultVoid::Success(); }

    ResultVoid MoveToPosition(const Point2D& target, float32 velocity) override {
        ++move_to_position_calls;
        last_target = target;
        last_velocity = velocity;
        position = target;
        moving = false;
        axis_statuses[0].state = MotionState::IDLE;
        axis_statuses[1].state = MotionState::IDLE;
        return ResultVoid::Success();
    }
    ResultVoid MoveAxisToPosition(LogicalAxisId axis, float32 target, float32 velocity) override {
        ++move_axis_calls;
        last_velocity = velocity;
        if (axis == LogicalAxisId::X) {
            position.x = target;
        } else if (axis == LogicalAxisId::Y) {
            position.y = target;
        }
        moving = false;
        return ResultVoid::Success();
    }
    ResultVoid RelativeMove(LogicalAxisId axis, float32 distance, float32 velocity) override {
        return MoveAxisToPosition(axis, axis == LogicalAxisId::Y ? position.y + distance : position.x + distance, velocity);
    }
    ResultVoid SynchronizedMove(const std::vector<MotionCommand>& commands) override {
        ++synchronized_move_calls;
        for (const auto& command : commands) {
            if (command.axis == LogicalAxisId::X) {
                position.x = command.position;
            } else if (command.axis == LogicalAxisId::Y) {
                position.y = command.position;
            }
        }
        moving = false;
        return ResultVoid::Success();
    }
    ResultVoid StopAxis(LogicalAxisId, bool) override { return StopAllAxes(true); }
    ResultVoid StopAllAxes(bool) override {
        ++stop_all_axes_calls;
        moving = false;
        axis_statuses[0].state = MotionState::IDLE;
        axis_statuses[1].state = MotionState::IDLE;
        return ResultVoid::Success();
    }
    ResultVoid EmergencyStop() override { ++emergency_stop_calls; return StopAllAxes(true); }
    ResultVoid WaitForMotionComplete(LogicalAxisId, int32) override {
        return moving ? ResultVoid::Failure(Error(ErrorCode::TIMEOUT, "still moving", "FakeMotionRuntimePort")) : ResultVoid::Success();
    }

    Result<Point2D> GetCurrentPosition() const override { return Result<Point2D>::Success(position); }
    Result<float32> GetAxisPosition(LogicalAxisId axis) const override { return Result<float32>::Success(axis == LogicalAxisId::Y ? position.y : position.x); }
    Result<float32> GetAxisVelocity(LogicalAxisId) const override { return Result<float32>::Success(0.0f); }
    Result<MotionStatus> GetAxisStatus(LogicalAxisId axis) const override { return Result<MotionStatus>::Success(axis == LogicalAxisId::Y ? axis_statuses[1] : axis_statuses[0]); }
    Result<bool> IsAxisMoving(LogicalAxisId) const override { return Result<bool>::Success(moving); }
    Result<bool> IsAxisInPosition(LogicalAxisId) const override { return Result<bool>::Success(!moving); }
    Result<std::vector<MotionStatus>> GetAllAxesStatus() const override { return Result<std::vector<MotionStatus>>::Success(axis_statuses); }

    ResultVoid StartJog(LogicalAxisId, int16, float32) override { return ResultVoid::Success(); }
    ResultVoid StartJogStep(LogicalAxisId axis, int16 direction, float32 distance, float32 velocity) override {
        return RelativeMove(axis, static_cast<float32>(direction) * distance, velocity);
    }
    ResultVoid StopJog(LogicalAxisId) override { return ResultVoid::Success(); }
    ResultVoid SetJogParameters(LogicalAxisId, const JogParameters&) override { return ResultVoid::Success(); }

    Result<IOStatus> ReadDigitalInput(int16 channel) override { return readIo(channel); }
    Result<IOStatus> ReadDigitalOutput(int16 channel) override { return readIo(channel); }
    ResultVoid WriteDigitalOutput(int16 channel, bool value) override {
        digital_outputs[channel] = value;
        ++write_digital_output_calls;
        last_output_channel = channel;
        return ResultVoid::Success();
    }
    Result<bool> ReadLimitStatus(LogicalAxisId, bool) override { return Result<bool>::Success(false); }
    Result<bool> ReadServoAlarm(LogicalAxisId) override { return Result<bool>::Success(false); }

    ResultVoid HomeAxis(LogicalAxisId axis) override { homed_axes[axis] = true; ++home_axis_calls; return ResultVoid::Success(); }
    ResultVoid StopHoming(LogicalAxisId) override { return ResultVoid::Success(); }
    ResultVoid ResetHomingState(LogicalAxisId axis) override { homed_axes[axis] = false; return ResultVoid::Success(); }
    Result<HomingStatus> GetHomingStatus(LogicalAxisId axis) const override {
        HomingStatus status;
        status.axis = axis;
        status.state = isAxisHomed(axis) ? HomingState::HOMED : HomingState::NOT_HOMED;
        return Result<HomingStatus>::Success(status);
    }
    Result<bool> IsAxisHomed(LogicalAxisId axis) const override { return Result<bool>::Success(isAxisHomed(axis)); }
    Result<bool> IsHomingInProgress(LogicalAxisId) const override { return Result<bool>::Success(false); }
    ResultVoid WaitForHomingComplete(LogicalAxisId, int32) override { return ResultVoid::Success(); }

    int move_to_position_calls = 0;
    int move_axis_calls = 0;
    int synchronized_move_calls = 0;
    int stop_all_axes_calls = 0;
    int emergency_stop_calls = 0;
    int home_axis_calls = 0;
    int write_digital_output_calls = 0;
    int16 last_output_channel = -1;
    Point2D last_target{0.0f, 0.0f};
    float32 last_velocity = 0.0f;
    Point2D position{0.0f, 0.0f};
    bool connected = true;
    bool moving = false;
    std::map<int16, bool> digital_outputs{};
    std::map<LogicalAxisId, bool> homed_axes{};
    std::vector<MotionStatus> axis_statuses{MotionStatus{}, MotionStatus{}};

private:
    bool isAxisHomed(LogicalAxisId axis) const {
        const auto it = homed_axes.find(axis);
        return it != homed_axes.end() && it->second;
    }
    Result<IOStatus> readIo(int16 channel) const {
        IOStatus status;
        status.channel = channel;
        const auto it = digital_outputs.find(channel);
        status.signal_active = it != digital_outputs.end() && it->second;
        status.value = status.signal_active ? 1U : 0U;
        return Result<IOStatus>::Success(status);
    }
};

class FakeConfigurationPort final : public IConfigurationPort {
public:
    explicit FakeConfigurationPort(std::size_t axis_count) {
        homing_configs.reserve(axis_count);
        for (std::size_t index = 0; index < axis_count; ++index) {
            HomingConfig config;
            config.axis = static_cast<int32>(index);
            config.rapid_velocity = 25.0f;
            config.locate_velocity = 10.0f;
            config.acceleration = 100.0f;
            config.deceleration = 100.0f;
            homing_configs.push_back(config);
        }
    }

    Result<Siligen::Domain::Configuration::Ports::SystemConfig> LoadConfiguration() override { return notImplemented<Siligen::Domain::Configuration::Ports::SystemConfig>("LoadConfiguration"); }
    ResultVoid SaveConfiguration(const Siligen::Domain::Configuration::Ports::SystemConfig&) override { return notImplementedVoid("SaveConfiguration"); }
    ResultVoid ReloadConfiguration() override { return notImplementedVoid("ReloadConfiguration"); }
    Result<Siligen::Domain::Configuration::Ports::DispensingConfig> GetDispensingConfig() const override { return Result<Siligen::Domain::Configuration::Ports::DispensingConfig>::Success({}); }
    ResultVoid SetDispensingConfig(const Siligen::Domain::Configuration::Ports::DispensingConfig&) override { return ResultVoid::Success(); }
    Result<Siligen::Domain::Configuration::Ports::DxfPreprocessConfig> GetDxfPreprocessConfig() const override { return Result<Siligen::Domain::Configuration::Ports::DxfPreprocessConfig>::Success({}); }
    Result<Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig> GetDxfTrajectoryConfig() const override { return Result<Siligen::Domain::Configuration::Ports::DxfTrajectoryConfig>::Success({}); }
    Result<Siligen::Shared::Types::DiagnosticsConfig> GetDiagnosticsConfig() const override { return Result<Siligen::Shared::Types::DiagnosticsConfig>::Success({}); }
    Result<Siligen::Domain::Configuration::Ports::MachineConfig> GetMachineConfig() const override { return Result<Siligen::Domain::Configuration::Ports::MachineConfig>::Success({}); }
    ResultVoid SetMachineConfig(const Siligen::Domain::Configuration::Ports::MachineConfig&) override { return ResultVoid::Success(); }
    Result<HomingConfig> GetHomingConfig(int axis) const override {
        if (axis < 0 || static_cast<std::size_t>(axis) >= homing_configs.size()) {
            return Result<HomingConfig>::Failure(Error(ErrorCode::INVALID_PARAMETER, "homing axis out of range", "FakeConfigurationPort"));
        }
        return Result<HomingConfig>::Success(homing_configs[static_cast<std::size_t>(axis)]);
    }
    ResultVoid SetHomingConfig(int axis, const HomingConfig& config) override {
        if (axis < 0 || static_cast<std::size_t>(axis) >= homing_configs.size()) {
            return ResultVoid::Failure(Error(ErrorCode::INVALID_PARAMETER, "homing axis out of range", "FakeConfigurationPort"));
        }
        homing_configs[static_cast<std::size_t>(axis)] = config;
        return ResultVoid::Success();
    }
    Result<std::vector<HomingConfig>> GetAllHomingConfigs() const override { return Result<std::vector<HomingConfig>>::Success(homing_configs); }
    Result<Siligen::Domain::Configuration::Ports::ValveSupplyConfig> GetValveSupplyConfig() const override { return Result<Siligen::Domain::Configuration::Ports::ValveSupplyConfig>::Success({}); }
    Result<Siligen::Shared::Types::DispenserValveConfig> GetDispenserValveConfig() const override { return Result<Siligen::Shared::Types::DispenserValveConfig>::Success({}); }
    Result<Siligen::Shared::Types::ValveCoordinationConfig> GetValveCoordinationConfig() const override { return Result<Siligen::Shared::Types::ValveCoordinationConfig>::Success({}); }
    Result<Siligen::Shared::Types::VelocityTraceConfig> GetVelocityTraceConfig() const override { return Result<Siligen::Shared::Types::VelocityTraceConfig>::Success({}); }
    Result<bool> ValidateConfiguration() const override { return Result<bool>::Success(true); }
    Result<std::vector<std::string>> GetValidationErrors() const override { return Result<std::vector<std::string>>::Success({}); }
    ResultVoid BackupConfiguration(const std::string&) override { return ResultVoid::Success(); }
    ResultVoid RestoreConfiguration(const std::string&) override { return ResultVoid::Success(); }
    Result<Siligen::Shared::Types::HardwareMode> GetHardwareMode() const override { return Result<Siligen::Shared::Types::HardwareMode>::Success(Siligen::Shared::Types::HardwareMode::Mock); }
    Result<Siligen::Shared::Types::HardwareConfiguration> GetHardwareConfiguration() const override { return Result<Siligen::Shared::Types::HardwareConfiguration>::Success({}); }

    std::vector<HomingConfig> homing_configs{};
};

class FakeEventPublisher final : public Siligen::Domain::System::Ports::IEventPublisherPort {
public:
    ResultVoid Publish(const DomainEvent& event) override { published_events.push_back(event.message); return ResultVoid::Success(); }
    ResultVoid PublishAsync(const DomainEvent& event) override { return Publish(event); }
    Result<int32> Subscribe(EventType, EventHandler) override { return Result<int32>::Success(1); }
    ResultVoid Unsubscribe(int32) override { return ResultVoid::Success(); }
    ResultVoid UnsubscribeAll(EventType) override { return ResultVoid::Success(); }
    Result<std::vector<DomainEvent*>> GetEventHistory(EventType, int32) const override { return Result<std::vector<DomainEvent*>>::Success({}); }
    ResultVoid ClearEventHistory() override { published_events.clear(); return ResultVoid::Success(); }

    std::vector<std::string> published_events{};
};

class FakeTriggerControllerPort final : public Siligen::Domain::Dispensing::Ports::ITriggerControllerPort {
public:
    ResultVoid ConfigureTrigger(const TriggerConfig& config) override { last_config = config; ++configure_calls; return ResultVoid::Success(); }
    Result<TriggerConfig> GetTriggerConfig() const override { return Result<TriggerConfig>::Success(last_config); }
    ResultVoid SetSingleTrigger(LogicalAxisId axis, float32 position, int32 pulse_width_us) override {
        last_axis = axis;
        last_position = position;
        last_pulse_width_us = pulse_width_us;
        ++single_trigger_calls;
        return ResultVoid::Success();
    }
    ResultVoid SetContinuousTrigger(LogicalAxisId, float32, float32, float32, int32) override { return ResultVoid::Success(); }
    ResultVoid SetRangeTrigger(LogicalAxisId, float32, float32, int32) override { return ResultVoid::Success(); }
    ResultVoid SetSequenceTrigger(LogicalAxisId, const std::vector<float32>&, int32) override { return ResultVoid::Success(); }
    ResultVoid EnableTrigger(LogicalAxisId) override { return ResultVoid::Success(); }
    ResultVoid DisableTrigger(LogicalAxisId) override { return ResultVoid::Success(); }
    ResultVoid ClearTrigger(LogicalAxisId) override { return ResultVoid::Success(); }
    Result<TriggerStatus> GetTriggerStatus(LogicalAxisId) const override { return Result<TriggerStatus>::Success({}); }
    Result<bool> IsTriggerEnabled(LogicalAxisId) const override { return Result<bool>::Success(true); }
    Result<int32> GetTriggerCount(LogicalAxisId) const override { return Result<int32>::Success(single_trigger_calls); }

    int configure_calls = 0;
    int single_trigger_calls = 0;
    TriggerConfig last_config{};
    LogicalAxisId last_axis = LogicalAxisId::X;
    float32 last_position = 0.0f;
    int32 last_pulse_width_us = 0;
};

MotionRuntimeAssemblyDependencies makeDependencies(
    const std::shared_ptr<FakeMotionRuntimePort>& motion_runtime_port,
    const std::shared_ptr<FakeInterpolationPort>& interpolation_port,
    const std::shared_ptr<FakeConfigurationPort>& configuration_port,
    const std::shared_ptr<FakeEventPublisher>& event_publisher,
    const std::shared_ptr<FakeTriggerControllerPort>& trigger_controller) {
    MotionRuntimeAssemblyDependencies dependencies;
    dependencies.motion_runtime_port = motion_runtime_port;
    dependencies.interpolation_port = interpolation_port;
    dependencies.configuration_port = configuration_port;
    dependencies.event_publisher_port = event_publisher;
    dependencies.trigger_controller_port = trigger_controller;
    return dependencies;
}

DeterministicPathExecutionRequest makePathRequest() {
    DeterministicPathExecutionRequest request;
    request.max_velocity_mm_s = 20.0f;
    request.max_acceleration_mm_s2 = 200.0f;
    request.sample_dt_s = 0.001f;
    request.axis_map = {LogicalAxisId::X, LogicalAxisId::Y};
    request.segments = {
        DeterministicPathSegment{DeterministicPathSegmentType::LINE, Point2D{5.0f, 0.0f}, Point2D{0.0f, 0.0f}},
        DeterministicPathSegment{DeterministicPathSegmentType::LINE, Point2D{5.0f, 5.0f}, Point2D{0.0f, 0.0f}},
    };
    return request;
}

void testFactoryCreatesAssembly() {
    auto motion_runtime_port = std::make_shared<FakeMotionRuntimePort>();
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto configuration_port = std::make_shared<FakeConfigurationPort>(2U);
    auto event_publisher = std::make_shared<FakeEventPublisher>();
    auto trigger_controller = std::make_shared<FakeTriggerControllerPort>();
    auto assembly = MotionRuntimeAssemblyFactory::Create(makeDependencies(
        motion_runtime_port, interpolation_port, configuration_port, event_publisher, trigger_controller));

    require(assembly.motion_control_service != nullptr, "factory should create motion control service");
    require(assembly.motion_status_service != nullptr, "factory should create motion status service");
    require(assembly.motion_validation_service != nullptr, "factory should create motion validation service");
    require(assembly.home_use_case != nullptr, "factory should create home use case");
    require(assembly.move_use_case != nullptr, "factory should create move use case");
    require(assembly.coordination_use_case != nullptr, "factory should create coordination use case");
    require(assembly.monitoring_use_case != nullptr, "factory should create monitoring use case");
    require(assembly.safety_use_case != nullptr, "factory should create safety use case");
    require(assembly.path_execution_use_case != nullptr, "factory should create path execution use case");
}

void testFactoryWiresMoveAndStop() {
    auto motion_runtime_port = std::make_shared<FakeMotionRuntimePort>();
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto configuration_port = std::make_shared<FakeConfigurationPort>(2U);
    auto event_publisher = std::make_shared<FakeEventPublisher>();
    auto trigger_controller = std::make_shared<FakeTriggerControllerPort>();
    auto assembly = MotionRuntimeAssemblyFactory::Create(makeDependencies(
        motion_runtime_port, interpolation_port, configuration_port, event_publisher, trigger_controller));

    MoveToPositionRequest request;
    request.target_position = Point2D{12.0f, 6.0f};
    request.movement_speed = 18.0f;
    request.validate_state = false;
    request.wait_for_completion = false;
    request.timeout_ms = 1;

    const auto move_result = assembly.move_use_case->Execute(request);
    require(move_result.IsSuccess(), "move use case should execute through runtime port");
    require(motion_runtime_port->move_to_position_calls == 1, "move should call motion runtime port once");
    require(motion_runtime_port->last_target.x == 12.0f, "move should keep X target");
    require(motion_runtime_port->last_target.y == 6.0f, "move should keep Y target");

    const auto stop_result = assembly.safety_use_case->StopAllAxes(true);
    require(stop_result.IsSuccess(), "safety use case should stop through runtime port");
    require(motion_runtime_port->stop_all_axes_calls == 1, "stop should call motion runtime port once");
}

void testFactoryWiresTriggerAndPathExecution() {
    auto motion_runtime_port = std::make_shared<FakeMotionRuntimePort>();
    auto interpolation_port = std::make_shared<FakeInterpolationPort>();
    auto configuration_port = std::make_shared<FakeConfigurationPort>(2U);
    auto event_publisher = std::make_shared<FakeEventPublisher>();
    auto trigger_controller = std::make_shared<FakeTriggerControllerPort>();
    auto assembly = MotionRuntimeAssemblyFactory::Create(makeDependencies(
        motion_runtime_port, interpolation_port, configuration_port, event_publisher, trigger_controller));

    MotionIOCommand command;
    command.channel = 4;
    command.type = MotionIOCommand::Type::COMPARE_OUTPUT;
    command.value = true;
    command.position = 1.25f;
    command.pulse_width_us = 9U;

    const auto compare_result = assembly.coordination_use_case->ConfigureCompareOutput(command);
    require(compare_result.IsSuccess(), "coordination should route compare output through trigger port");
    require(trigger_controller->configure_calls == 1, "compare output should configure trigger");
    require(trigger_controller->single_trigger_calls == 1, "compare output should arm single trigger");
    require(trigger_controller->last_config.channel == 4, "compare output should preserve channel");
    require(trigger_controller->last_position == 1.25f, "compare output should preserve trigger position");

    const auto start_result = assembly.path_execution_use_case->Start(makePathRequest());
    require(start_result.IsSuccess(), "path execution should start from factory assembly");
    require(start_result.Value().state == DeterministicPathExecutionState::READY, "path start should stay non-blocking");

    const auto advance_result = assembly.path_execution_use_case->Advance();
    require(advance_result.IsSuccess(), "path execution advance should succeed");
    require(interpolation_port->configure_calls >= 1, "path advance should configure interpolation");
    require(interpolation_port->add_calls >= 1, "path advance should stage interpolation data");
    require(interpolation_port->flush_calls >= 1, "path advance should flush interpolation data");
    require(interpolation_port->start_calls >= 1, "path advance should service coordinate-system start");
}

}  // namespace

int main() {
    try {
        testFactoryCreatesAssembly();
        testFactoryWiresMoveAndStop();
        testFactoryWiresTriggerAndPathExecution();
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
