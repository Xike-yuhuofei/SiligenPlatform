#include "sim/scheme_c/process_runtime_shim.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "domain/motion/value-objects/MotionTypes.h"
#include "shared/types/AxisTypes.h"
#include "shared/types/Error.h"
#include "shared/types/Point.h"
#include "shared/types/Result.h"

namespace sim::scheme_c {
namespace {

using AxisConfiguration = Siligen::Domain::Motion::Ports::AxisConfiguration;
using CoordinateSystemConfig = Siligen::Domain::Motion::Ports::CoordinateSystemConfig;
using CoordinateSystemRuntimeStatus = Siligen::Domain::Motion::Ports::CoordinateSystemStatus;
using CoordinateSystemState = Siligen::Domain::Motion::Ports::CoordinateSystemState;
using Error = Siligen::Shared::Types::Error;
using ErrorCode = Siligen::Shared::Types::ErrorCode;
using HomingStage = Siligen::Domain::Motion::ValueObjects::HomingStage;
using HomingState = Siligen::Domain::Motion::Ports::HomingState;
using HomingStatus = Siligen::Domain::Motion::Ports::HomingStatus;
using IAxisControlPort = Siligen::Domain::Motion::Ports::IAxisControlPort;
using IHomingPort = Siligen::Domain::Motion::Ports::IHomingPort;
using IInterpolationPort = Siligen::Domain::Motion::Ports::IInterpolationPort;
using IIOControlPort = Siligen::Domain::Motion::Ports::IIOControlPort;
using IMotionStatePort = Siligen::Domain::Motion::Ports::IMotionStatePort;
using InterpolationData = Siligen::Domain::Motion::Ports::InterpolationData;
using IOStatus = Siligen::Domain::Motion::Ports::IOStatus;
using LogicalAxisId = Siligen::Shared::Types::LogicalAxisId;
using MotionState = Siligen::Domain::Motion::Ports::MotionState;
using MotionStatus = Siligen::Domain::Motion::Ports::MotionStatus;
using Point2D = Siligen::Shared::Types::Point2D;
template <typename T>
using Result = Siligen::Shared::Types::Result<T>;
using ResultVoid = Siligen::Shared::Types::Result<void>;
using Siligen::Shared::Types::FromIndex;
using Siligen::Shared::Types::IsValid;
using Siligen::Shared::Types::ToIndex;

Error makeError(ErrorCode code, const std::string& message) {
    return Error(code, message, "simulation-engine/process-runtime-shim");
}

std::vector<std::string> axisNamesFromSnapshot(const VirtualAxisGroup& axis_group) {
    std::vector<std::string> names;
    const auto states = axis_group.snapshot();
    names.reserve(states.size());

    for (const auto& state : states) {
        names.push_back(state.axis);
    }

    return names;
}

std::vector<std::string> ioNamesFromSnapshot(const VirtualIo& io) {
    std::vector<std::string> names;
    const auto states = io.snapshot();
    names.reserve(states.size());

    for (const auto& state : states) {
        names.push_back(state.channel);
    }

    return names;
}

RuntimeBridgeBindings normalizeBindings(
    const RuntimeBridgeBindings& bindings,
    const VirtualAxisGroup& axis_group,
    const VirtualIo& io) {
    RuntimeBridgeBindings normalized = bindings;
    const auto defaults = makeDefaultRuntimeBridgeBindings(
        axisNamesFromSnapshot(axis_group),
        ioNamesFromSnapshot(io));

    if (normalized.axis_bindings.empty()) {
        normalized.axis_bindings = defaults.axis_bindings;
    }
    if (normalized.io_bindings.empty()) {
        normalized.io_bindings = defaults.io_bindings;
    }

    return normalized;
}

class BindingResolver {
public:
    explicit BindingResolver(RuntimeBridgeBindings bindings)
        : bindings_(std::move(bindings)) {
        for (const auto& binding : bindings_.axis_bindings) {
            axis_by_index_.emplace(binding.logical_axis_index, binding.axis_name);
        }
        for (const auto& binding : bindings_.io_bindings) {
            io_by_channel_.emplace(binding.channel, binding.signal_name);
        }
    }

    const RuntimeBridgeBindings& bindings() const {
        return bindings_;
    }

    Result<std::string> axisName(LogicalAxisId axis) const {
        const auto axis_index = static_cast<std::int32_t>(ToIndex(axis));
        const auto it = axis_by_index_.find(axis_index);
        if (it == axis_by_index_.end()) {
            return Result<std::string>::Failure(makeError(ErrorCode::INVALID_PARAMETER, "Axis binding not found"));
        }

        return Result<std::string>::Success(it->second);
    }

    std::vector<LogicalAxisId> allAxes() const {
        std::vector<LogicalAxisId> axes;
        axes.reserve(bindings_.axis_bindings.size());

        for (const auto& binding : bindings_.axis_bindings) {
            const auto axis = FromIndex(static_cast<int16>(binding.logical_axis_index));
            if (IsValid(axis)) {
                axes.push_back(axis);
            }
        }

        return axes;
    }

    Result<std::string> ioName(std::int16_t channel) const {
        const auto it = io_by_channel_.find(channel);
        if (it == io_by_channel_.end()) {
            return Result<std::string>::Failure(makeError(ErrorCode::INVALID_PARAMETER, "IO binding not found"));
        }

        return Result<std::string>::Success(it->second);
    }

private:
    RuntimeBridgeBindings bindings_{};
    std::unordered_map<std::int32_t, std::string> axis_by_index_{};
    std::unordered_map<std::int32_t, std::string> io_by_channel_{};
};

struct AxisHardwareConfig {
    short positive_io_index{-1};
    short negative_io_index{-1};
    short positive_polarity{0};
    short negative_polarity{0};
    bool positive_enabled{true};
    bool negative_enabled{true};
};

struct CoordinateSystemRuntime {
    CoordinateSystemConfig config{};
    std::vector<InterpolationData> buffer{};
    float override_percent{100.0f};
    bool s_curve_enabled{false};
    bool constant_linear_velocity{false};
};

class BridgeContext {
public:
    BridgeContext(VirtualAxisGroup& axis_group, VirtualIo& io, RuntimeBridgeBindings bindings)
        : axis_group_(axis_group),
          io_(io),
          resolver_(std::move(bindings)) {}

    const RuntimeBridgeBindings& bindings() const {
        return resolver_.bindings();
    }

    Result<AxisState> readAxis(LogicalAxisId axis) const {
        auto axis_name = resolver_.axisName(axis);
        if (axis_name.IsError()) {
            return Result<AxisState>::Failure(axis_name.GetError());
        }
        if (!axis_group_.hasAxis(axis_name.Value())) {
            return Result<AxisState>::Failure(makeError(ErrorCode::INVALID_PARAMETER, "Virtual axis not found"));
        }

        return Result<AxisState>::Success(axis_group_.readAxis(axis_name.Value()));
    }

    ResultVoid writeAxis(const AxisState& state) {
        axis_group_.writeAxisState(state);
        return ResultVoid::Success();
    }

    ResultVoid applyCommands(const std::vector<AxisCommand>& commands) {
        axis_group_.applyCommands(commands);
        return ResultVoid::Success();
    }

    Result<IoState> readIo(std::int16_t channel) const {
        auto io_name = resolver_.ioName(channel);
        if (io_name.IsError()) {
            return Result<IoState>::Failure(io_name.GetError());
        }
        if (!io_.hasChannel(io_name.Value())) {
            return Result<IoState>::Failure(makeError(ErrorCode::INVALID_PARAMETER, "Virtual IO channel not found"));
        }

        return Result<IoState>::Success(io_.readChannel(io_name.Value()));
    }

    ResultVoid writeIo(std::int16_t channel, bool value, bool output) {
        auto io_name = resolver_.ioName(channel);
        if (io_name.IsError()) {
            return ResultVoid::Failure(io_name.GetError());
        }

        if (output) {
            io_.writeOutput(io_name.Value(), value);
            return ResultVoid::Success();
        }

        IoState state = io_.readChannel(io_name.Value());
        state.channel = io_name.Value();
        state.value = value;
        state.output = output;
        io_.writeChannelState(state);
        return ResultVoid::Success();
    }

    Point2D currentPosition() const {
        Point2D position;

        const auto x_axis = readAxis(LogicalAxisId::X);
        if (x_axis.IsSuccess()) {
            position.x = static_cast<float>(x_axis.Value().position_mm);
        }

        const auto y_axis = readAxis(LogicalAxisId::Y);
        if (y_axis.IsSuccess()) {
            position.y = static_cast<float>(y_axis.Value().position_mm);
        }

        return position;
    }

    CoordinateSystemRuntime& coordinateSystem(std::int16_t coord_sys) {
        return coordinate_systems_[coord_sys];
    }

    Result<CoordinateSystemRuntime*> findCoordinateSystem(std::int16_t coord_sys) {
        auto it = coordinate_systems_.find(coord_sys);
        if (it == coordinate_systems_.end()) {
            return Result<CoordinateSystemRuntime*>::Failure(
                makeError(ErrorCode::INVALID_PARAMETER, "Coordinate system not configured"));
        }

        return Result<CoordinateSystemRuntime*>::Success(&it->second);
    }

    Result<const CoordinateSystemRuntime*> findCoordinateSystem(std::int16_t coord_sys) const {
        auto it = coordinate_systems_.find(coord_sys);
        if (it == coordinate_systems_.end()) {
            return Result<const CoordinateSystemRuntime*>::Failure(
                makeError(ErrorCode::INVALID_PARAMETER, "Coordinate system not configured"));
        }

        return Result<const CoordinateSystemRuntime*>::Success(&it->second);
    }

    AxisHardwareConfig& axisHardware(LogicalAxisId axis) {
        return axis_hardware_[static_cast<std::int32_t>(ToIndex(axis))];
    }

    std::vector<LogicalAxisId> allAxes() const {
        return resolver_.allAxes();
    }

    Result<std::string> axisName(LogicalAxisId axis) const {
        return resolver_.axisName(axis);
    }

private:
    VirtualAxisGroup& axis_group_;
    VirtualIo& io_;
    BindingResolver resolver_;
    std::unordered_map<std::int16_t, CoordinateSystemRuntime> coordinate_systems_{};
    std::unordered_map<std::int32_t, AxisHardwareConfig> axis_hardware_{};
};

MotionState toMotionState(const AxisState& state) {
    if (state.has_error || state.servo_alarm) {
        return MotionState::FAULT;
    }
    if (!state.enabled) {
        return MotionState::DISABLED;
    }
    if (state.running) {
        return MotionState::MOVING;
    }
    if (state.homed) {
        return MotionState::HOMED;
    }
    if (state.done) {
        return MotionState::IDLE;
    }

    return MotionState::STOPPED;
}

class MotionStatePortShim final : public IMotionStatePort {
public:
    explicit MotionStatePortShim(std::shared_ptr<BridgeContext> context)
        : context_(std::move(context)) {}

    Result<Point2D> GetCurrentPosition() const override {
        return Result<Point2D>::Success(context_->currentPosition());
    }

    Result<float32> GetAxisPosition(LogicalAxisId axis) const override {
        const auto state = context_->readAxis(axis);
        if (state.IsError()) {
            return Result<float32>::Failure(state.GetError());
        }

        return Result<float32>::Success(static_cast<float32>(state.Value().position_mm));
    }

    Result<float32> GetAxisVelocity(LogicalAxisId axis) const override {
        const auto state = context_->readAxis(axis);
        if (state.IsError()) {
            return Result<float32>::Failure(state.GetError());
        }

        return Result<float32>::Success(static_cast<float32>(state.Value().velocity_mm_per_s));
    }

    Result<MotionStatus> GetAxisStatus(LogicalAxisId axis) const override {
        const auto axis_state = context_->readAxis(axis);
        if (axis_state.IsError()) {
            return Result<MotionStatus>::Failure(axis_state.GetError());
        }

        MotionStatus status;
        status.state = toMotionState(axis_state.Value());
        status.position = context_->currentPosition();
        status.velocity = static_cast<float32>(axis_state.Value().velocity_mm_per_s);
        status.acceleration = static_cast<float32>(axis_state.Value().configured_acceleration_mm_per_s2);
        status.in_position = axis_state.Value().done;
        status.has_error = axis_state.Value().has_error;
        status.error_code = axis_state.Value().error_code;
        status.enabled = axis_state.Value().enabled;
        status.following_error = false;
        status.soft_limit_positive = axis_state.Value().positive_soft_limit;
        status.soft_limit_negative = axis_state.Value().negative_soft_limit;
        status.hard_limit_positive = axis_state.Value().positive_hard_limit;
        status.hard_limit_negative = axis_state.Value().negative_hard_limit;
        status.servo_alarm = axis_state.Value().servo_alarm;
        status.home_signal = axis_state.Value().home_signal;
        status.index_signal = axis_state.Value().index_signal;
        return Result<MotionStatus>::Success(status);
    }

    Result<bool> IsAxisMoving(LogicalAxisId axis) const override {
        const auto state = context_->readAxis(axis);
        if (state.IsError()) {
            return Result<bool>::Failure(state.GetError());
        }

        return Result<bool>::Success(state.Value().running);
    }

    Result<bool> IsAxisInPosition(LogicalAxisId axis) const override {
        const auto state = context_->readAxis(axis);
        if (state.IsError()) {
            return Result<bool>::Failure(state.GetError());
        }

        return Result<bool>::Success(state.Value().done);
    }

    Result<std::vector<MotionStatus>> GetAllAxesStatus() const override {
        std::vector<MotionStatus> all_status;
        for (const auto axis : context_->allAxes()) {
            const auto axis_status = GetAxisStatus(axis);
            if (axis_status.IsSuccess()) {
                all_status.push_back(axis_status.Value());
            }
        }

        return Result<std::vector<MotionStatus>>::Success(all_status);
    }

private:
    std::shared_ptr<BridgeContext> context_;
};

class IoControlPortShim final : public IIOControlPort {
public:
    explicit IoControlPortShim(std::shared_ptr<BridgeContext> context)
        : context_(std::move(context)) {}

    Result<IOStatus> ReadDigitalInput(int16 channel) override {
        return toStatus(channel);
    }

    Result<IOStatus> ReadDigitalOutput(int16 channel) override {
        return toStatus(channel);
    }

    ResultVoid WriteDigitalOutput(int16 channel, bool value) override {
        return context_->writeIo(channel, value, true);
    }

    Result<bool> ReadLimitStatus(LogicalAxisId axis, bool positive) override {
        const auto state = context_->readAxis(axis);
        if (state.IsError()) {
            return Result<bool>::Failure(state.GetError());
        }

        const bool limit = positive
            ? (state.Value().positive_soft_limit || state.Value().positive_hard_limit)
            : (state.Value().negative_soft_limit || state.Value().negative_hard_limit);
        return Result<bool>::Success(limit);
    }

    Result<bool> ReadServoAlarm(LogicalAxisId axis) override {
        const auto state = context_->readAxis(axis);
        if (state.IsError()) {
            return Result<bool>::Failure(state.GetError());
        }

        return Result<bool>::Success(state.Value().servo_alarm);
    }

private:
    Result<IOStatus> toStatus(int16 channel) const {
        const auto io_state = context_->readIo(channel);
        if (io_state.IsError()) {
            return Result<IOStatus>::Failure(io_state.GetError());
        }

        IOStatus status;
        status.channel = channel;
        status.signal_active = io_state.Value().value;
        status.value = io_state.Value().value ? 1U : 0U;
        status.timestamp = 0;
        return Result<IOStatus>::Success(status);
    }

    std::shared_ptr<BridgeContext> context_;
};

class HomingPortShim final : public IHomingPort {
public:
    explicit HomingPortShim(std::shared_ptr<BridgeContext> context)
        : context_(std::move(context)) {}

    ResultVoid HomeAxis(LogicalAxisId axis) override {
        const auto current = context_->readAxis(axis);
        if (current.IsError()) {
            return ResultVoid::Failure(current.GetError());
        }

        const auto axis_name = context_->axisName(axis);
        if (axis_name.IsError()) {
            return ResultVoid::Failure(axis_name.GetError());
        }

        const auto apply = context_->applyCommands({AxisCommand{axis_name.Value(), 0.0, 0.0, true, false}});
        if (apply.IsError()) {
            return apply;
        }
        return ResultVoid::Success();
    }

    ResultVoid StopHoming(LogicalAxisId axis) override {
        const auto current = context_->readAxis(axis);
        if (current.IsError()) {
            return ResultVoid::Failure(current.GetError());
        }

        const auto axis_name = context_->axisName(axis);
        if (axis_name.IsError()) {
            return ResultVoid::Failure(axis_name.GetError());
        }

        const auto apply = context_->applyCommands({AxisCommand{
            axis_name.Value(),
            current.Value().position_mm,
            0.0,
            false,
            true
        }});
        if (apply.IsError()) {
            return apply;
        }
        return ResultVoid::Success();
    }

    ResultVoid ResetHomingState(LogicalAxisId axis) override {
        const auto current = context_->readAxis(axis);
        if (current.IsError()) {
            return ResultVoid::Failure(current.GetError());
        }

        AxisState state = current.Value();
        state.homed = false;
        state.home_signal = false;
        return context_->writeAxis(state);
    }

    Result<HomingStatus> GetHomingStatus(LogicalAxisId axis) const override {
        const auto current = context_->readAxis(axis);
        if (current.IsError()) {
            return Result<HomingStatus>::Failure(current.GetError());
        }

        HomingStatus status;
        status.axis = axis;
        if (current.Value().has_error || current.Value().servo_alarm) {
            status.state = HomingState::FAILED;
            status.stage = HomingStage::Failed;
        } else if (current.Value().homed) {
            status.state = HomingState::HOMED;
            status.stage = HomingStage::Completed;
        } else if (current.Value().running) {
            status.state = HomingState::HOMING;
            status.stage = HomingStage::SearchingHome;
        } else {
            status.state = HomingState::NOT_HOMED;
            status.stage = HomingStage::Idle;
        }
        status.current_position = static_cast<float32>(current.Value().position_mm);
        status.error_code = current.Value().error_code;
        status.is_searching = current.Value().running;
        return Result<HomingStatus>::Success(status);
    }

    Result<bool> IsAxisHomed(LogicalAxisId axis) const override {
        const auto current = context_->readAxis(axis);
        if (current.IsError()) {
            return Result<bool>::Failure(current.GetError());
        }

        return Result<bool>::Success(current.Value().homed);
    }

    Result<bool> IsHomingInProgress(LogicalAxisId axis) const override {
        const auto current = context_->readAxis(axis);
        if (current.IsError()) {
            return Result<bool>::Failure(current.GetError());
        }

        return Result<bool>::Success(current.Value().running && !current.Value().homed);
    }

    ResultVoid WaitForHomingComplete(LogicalAxisId axis, int32 /*timeout_ms*/) override {
        const auto homed = IsAxisHomed(axis);
        if (homed.IsError()) {
            return ResultVoid::Failure(homed.GetError());
        }
        if (!homed.Value()) {
            return ResultVoid::Failure(makeError(ErrorCode::TIMEOUT, "Axis homing not completed"));
        }

        return ResultVoid::Success();
    }

private:
    std::shared_ptr<BridgeContext> context_;
};

class AxisControlPortShim final : public IAxisControlPort {
public:
    explicit AxisControlPortShim(std::shared_ptr<BridgeContext> context)
        : context_(std::move(context)) {}

    ResultVoid EnableAxis(LogicalAxisId axis) override {
        return updateAxis(axis, [](AxisState& state) {
            state.enabled = true;
            state.has_error = false;
            state.error_code = 0;
        });
    }

    ResultVoid DisableAxis(LogicalAxisId axis) override {
        return updateAxis(axis, [](AxisState& state) {
            state.enabled = false;
            state.running = false;
            state.done = true;
        });
    }

    Result<bool> IsAxisEnabled(LogicalAxisId axis) const override {
        const auto state = context_->readAxis(axis);
        if (state.IsError()) {
            return Result<bool>::Failure(state.GetError());
        }

        return Result<bool>::Success(state.Value().enabled);
    }

    ResultVoid ClearAxisStatus(LogicalAxisId axis) override {
        return updateAxis(axis, [](AxisState& state) {
            state.has_error = false;
            state.error_code = 0;
            state.servo_alarm = false;
        });
    }

    ResultVoid ClearPosition(LogicalAxisId axis) override {
        return updateAxis(axis, [](AxisState& state) {
            state.position_mm = 0.0;
        });
    }

    ResultVoid SetAxisVelocity(LogicalAxisId axis, float32 velocity) override {
        return updateAxis(axis, [velocity](AxisState& state) {
            state.configured_velocity_mm_per_s = velocity;
        });
    }

    ResultVoid SetAxisAcceleration(LogicalAxisId axis, float32 acceleration) override {
        return updateAxis(axis, [acceleration](AxisState& state) {
            state.configured_acceleration_mm_per_s2 = acceleration;
        });
    }

    ResultVoid SetSoftLimits(LogicalAxisId axis, float32 negative_limit, float32 positive_limit) override {
        if (negative_limit > positive_limit) {
            return ResultVoid::Failure(makeError(ErrorCode::INVALID_PARAMETER, "Invalid soft limit range"));
        }

        return updateAxis(axis, [negative_limit, positive_limit](AxisState& state) {
            state.soft_limit_negative_mm = negative_limit;
            state.soft_limit_positive_mm = positive_limit;
        });
    }

    ResultVoid ConfigureAxis(LogicalAxisId axis, const AxisConfiguration& config) override {
        return updateAxis(axis, [&config](AxisState& state) {
            state.configured_velocity_mm_per_s = config.max_velocity;
            state.configured_acceleration_mm_per_s2 = config.max_acceleration;
            state.soft_limit_negative_mm = config.soft_limit_negative;
            state.soft_limit_positive_mm = config.soft_limit_positive;
            state.positive_hard_limit_enabled = config.hard_limits_enabled;
            state.negative_hard_limit_enabled = config.hard_limits_enabled;
        });
    }

    ResultVoid SetHardLimits(LogicalAxisId axis,
                             short positive_io_index,
                             short negative_io_index,
                             short /*card_index*/,
                             short /*signal_type*/) override {
        auto& hardware = context_->axisHardware(axis);
        hardware.positive_io_index = positive_io_index;
        hardware.negative_io_index = negative_io_index;
        return ResultVoid::Success();
    }

    ResultVoid EnableHardLimits(LogicalAxisId axis, bool enable, short limit_type) override {
        auto& hardware = context_->axisHardware(axis);
        if (limit_type == 0 || limit_type == -1) {
            hardware.positive_enabled = enable;
        }
        if (limit_type == 1 || limit_type == -1) {
            hardware.negative_enabled = enable;
        }

        return updateAxis(axis, [hardware](AxisState& state) {
            state.positive_hard_limit_enabled = hardware.positive_enabled;
            state.negative_hard_limit_enabled = hardware.negative_enabled;
        });
    }

    ResultVoid SetHardLimitPolarity(LogicalAxisId axis,
                                    short positive_polarity,
                                    short negative_polarity) override {
        auto& hardware = context_->axisHardware(axis);
        hardware.positive_polarity = positive_polarity;
        hardware.negative_polarity = negative_polarity;
        return ResultVoid::Success();
    }

private:
    template <typename Fn>
    ResultVoid updateAxis(LogicalAxisId axis, Fn&& fn) const {
        const auto current = context_->readAxis(axis);
        if (current.IsError()) {
            return ResultVoid::Failure(current.GetError());
        }

        AxisState state = current.Value();
        fn(state);
        return context_->writeAxis(state);
    }

    std::shared_ptr<BridgeContext> context_;
};

class InterpolationPortShim final : public IInterpolationPort {
public:
    explicit InterpolationPortShim(std::shared_ptr<BridgeContext> context)
        : context_(std::move(context)) {}

    ResultVoid ConfigureCoordinateSystem(int16 coord_sys, const CoordinateSystemConfig& config) override {
        if (config.axis_map.empty()) {
            return ResultVoid::Failure(makeError(ErrorCode::INVALID_PARAMETER, "Coordinate system axis map is empty"));
        }

        auto& runtime = context_->coordinateSystem(coord_sys);
        runtime.config = config;
        return ResultVoid::Success();
    }

    ResultVoid AddInterpolationData(int16 coord_sys, const InterpolationData& data) override {
        auto runtime = context_->findCoordinateSystem(coord_sys);
        if (runtime.IsError()) {
            return ResultVoid::Failure(runtime.GetError());
        }

        runtime.Value()->buffer.push_back(data);
        return ResultVoid::Success();
    }

    ResultVoid ClearInterpolationBuffer(int16 coord_sys) override {
        auto runtime = context_->findCoordinateSystem(coord_sys);
        if (runtime.IsError()) {
            return ResultVoid::Failure(runtime.GetError());
        }

        runtime.Value()->buffer.clear();
        return ResultVoid::Success();
    }

    ResultVoid FlushInterpolationData(int16 coord_sys) override {
        auto runtime = context_->findCoordinateSystem(coord_sys);
        if (runtime.IsError()) {
            return ResultVoid::Failure(runtime.GetError());
        }

        return ResultVoid::Success();
    }

    ResultVoid StartCoordinateSystemMotion(uint32 coord_sys_mask) override {
        for (const auto coord_sys : activeCoordinateSystems(coord_sys_mask)) {
            auto runtime = context_->findCoordinateSystem(coord_sys);
            if (runtime.IsError()) {
                return ResultVoid::Failure(runtime.GetError());
            }

            if (runtime.Value()->buffer.empty()) {
                continue;
            }

            const auto& final_segment = runtime.Value()->buffer.back();
            std::vector<AxisCommand> commands;
            const auto axis_count = std::min(runtime.Value()->config.axis_map.size(), final_segment.positions.size());
            commands.reserve(axis_count);

            for (std::size_t index = 0; index < axis_count; ++index) {
                const auto axis_name = context_->axisName(runtime.Value()->config.axis_map[index]);
                if (axis_name.IsError()) {
                    return ResultVoid::Failure(axis_name.GetError());
                }

                commands.push_back(AxisCommand{
                    axis_name.Value(),
                    final_segment.positions[index],
                    final_segment.velocity * (runtime.Value()->override_percent / 100.0f),
                    false,
                    false
                });
            }

            const auto apply = context_->applyCommands(commands);
            if (apply.IsError()) {
                return apply;
            }

            runtime.Value()->buffer.clear();
        }

        return ResultVoid::Success();
    }

    ResultVoid StopCoordinateSystemMotion(uint32 coord_sys_mask) override {
        for (const auto coord_sys : activeCoordinateSystems(coord_sys_mask)) {
            auto runtime = context_->findCoordinateSystem(coord_sys);
            if (runtime.IsError()) {
                return ResultVoid::Failure(runtime.GetError());
            }

            std::vector<AxisCommand> commands;
            for (const auto axis : runtime.Value()->config.axis_map) {
                const auto axis_name = context_->axisName(axis);
                if (axis_name.IsError()) {
                    return ResultVoid::Failure(axis_name.GetError());
                }

                commands.push_back(AxisCommand{axis_name.Value(), 0.0, 0.0, false, true});
            }

            const auto apply = context_->applyCommands(commands);
            if (apply.IsError()) {
                return apply;
            }
        }

        return ResultVoid::Success();
    }

    ResultVoid SetCoordinateSystemVelocityOverride(int16 coord_sys, float32 override_percent) override {
        auto runtime = context_->findCoordinateSystem(coord_sys);
        if (runtime.IsError()) {
            return ResultVoid::Failure(runtime.GetError());
        }

        runtime.Value()->override_percent = override_percent;
        return ResultVoid::Success();
    }

    ResultVoid EnableCoordinateSystemSCurve(int16 coord_sys, float32 /*jerk*/) override {
        auto runtime = context_->findCoordinateSystem(coord_sys);
        if (runtime.IsError()) {
            return ResultVoid::Failure(runtime.GetError());
        }

        runtime.Value()->s_curve_enabled = true;
        return ResultVoid::Success();
    }

    ResultVoid DisableCoordinateSystemSCurve(int16 coord_sys) override {
        auto runtime = context_->findCoordinateSystem(coord_sys);
        if (runtime.IsError()) {
            return ResultVoid::Failure(runtime.GetError());
        }

        runtime.Value()->s_curve_enabled = false;
        return ResultVoid::Success();
    }

    ResultVoid SetConstLinearVelocityMode(int16 coord_sys, bool enabled, uint32 /*rotate_axis_mask*/) override {
        auto runtime = context_->findCoordinateSystem(coord_sys);
        if (runtime.IsError()) {
            return ResultVoid::Failure(runtime.GetError());
        }

        runtime.Value()->constant_linear_velocity = enabled;
        return ResultVoid::Success();
    }

    Result<uint32> GetInterpolationBufferSpace(int16 coord_sys) const override {
        auto runtime = context_->findCoordinateSystem(coord_sys);
        if (runtime.IsError()) {
            return Result<uint32>::Failure(runtime.GetError());
        }

        return Result<uint32>::Success(1024U - static_cast<uint32>(runtime.Value()->buffer.size()));
    }

    Result<uint32> GetLookAheadBufferSpace(int16 coord_sys) const override {
        return GetInterpolationBufferSpace(coord_sys);
    }

    Result<CoordinateSystemRuntimeStatus> GetCoordinateSystemStatus(int16 coord_sys) const override {
        auto runtime = context_->findCoordinateSystem(coord_sys);
        if (runtime.IsError()) {
            return Result<CoordinateSystemRuntimeStatus>::Failure(runtime.GetError());
        }

        CoordinateSystemRuntimeStatus status;
        status.remaining_segments = static_cast<uint32>(runtime.Value()->buffer.size());
        status.current_velocity = 0.0f;
        status.raw_status_word = 0;
        status.raw_segment = static_cast<int32>(runtime.Value()->buffer.size());
        status.mc_status_ret = 0;

        bool any_running = false;
        for (const auto axis : runtime.Value()->config.axis_map) {
            const auto state = context_->readAxis(axis);
            if (state.IsSuccess() && state.Value().running) {
                any_running = true;
                status.current_velocity = static_cast<float32>(state.Value().velocity_mm_per_s);
            }
        }

        status.is_moving = any_running;
        status.state = any_running ? CoordinateSystemState::MOVING : CoordinateSystemState::IDLE;
        return Result<CoordinateSystemRuntimeStatus>::Success(status);
    }

private:
    std::vector<int16> activeCoordinateSystems(uint32 coord_sys_mask) const {
        std::vector<int16> systems;
        for (int16 coord_sys = 1; coord_sys <= 16; ++coord_sys) {
            const uint32 mask = 1U << static_cast<uint32>(coord_sys - 1);
            if ((coord_sys_mask & mask) != 0U) {
                systems.push_back(coord_sys);
            }
        }

        return systems;
    }

    std::shared_ptr<BridgeContext> context_;
};

}  // namespace

ProcessRuntimeCoreShimBundle createProcessRuntimeCoreShimBundle(
    VirtualAxisGroup& axis_group,
    VirtualIo& io,
    const RuntimeBridgeBindings& bindings) {
    ProcessRuntimeCoreShimBundle bundle;
    bundle.resolved_bindings = normalizeBindings(bindings, axis_group, io);

    auto context = std::make_shared<BridgeContext>(axis_group, io, bundle.resolved_bindings);
    bundle.homing = std::make_shared<HomingPortShim>(context);
    bundle.interpolation = std::make_shared<InterpolationPortShim>(context);
    bundle.axis_control = std::make_shared<AxisControlPortShim>(context);
    bundle.motion_state = std::make_shared<MotionStatePortShim>(context);
    bundle.io_control = std::make_shared<IoControlPortShim>(context);
    return bundle;
}

}  // namespace sim::scheme_c
