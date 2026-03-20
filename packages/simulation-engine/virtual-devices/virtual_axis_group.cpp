#include "sim/scheme_c/virtual_axis_group.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sim::scheme_c {
namespace {

constexpr double kEpsilon = 1e-9;
constexpr double kVelocityEpsilon = 1e-6;
constexpr double kDefaultVelocityMmPerSecond = 50.0;
constexpr double kDefaultFollowingErrorThresholdMm = 1e-4;
constexpr double kHomePositionMm = 0.0;

constexpr int kAxisErrorDisabled = 1001;
constexpr int kAxisErrorPositiveSoftLimit = 1002;
constexpr int kAxisErrorNegativeSoftLimit = 1003;
constexpr int kAxisErrorPositiveHardLimit = 1004;
constexpr int kAxisErrorNegativeHardLimit = 1005;

double tickSeconds(const TickInfo& tick) {
    const auto seconds = std::chrono::duration<double>(tick.step).count();
    return seconds > 0.0 ? seconds : 0.0;
}

bool nearlyEqual(double lhs, double rhs) {
    return std::abs(lhs - rhs) <= kEpsilon;
}

bool hasMeaningfulVelocity(double value) {
    return std::abs(value) > kEpsilon;
}

double commandVelocity(const AxisState& state, const AxisCommand& command) {
    if (hasMeaningfulVelocity(command.target_velocity_mm_per_s)) {
        return std::abs(command.target_velocity_mm_per_s);
    }
    if (hasMeaningfulVelocity(state.configured_velocity_mm_per_s)) {
        return std::abs(state.configured_velocity_mm_per_s);
    }

    return kDefaultVelocityMmPerSecond;
}

bool commandsEquivalent(const AxisCommand& lhs, const AxisCommand& rhs) {
    return lhs.axis == rhs.axis &&
        nearlyEqual(lhs.target_position_mm, rhs.target_position_mm) &&
        nearlyEqual(lhs.target_velocity_mm_per_s, rhs.target_velocity_mm_per_s) &&
        lhs.home_requested == rhs.home_requested &&
        lhs.stop_requested == rhs.stop_requested;
}

bool motionRealismEnabled(const MotionRealismConfig& config) {
    return config.enabled &&
        (config.servo_lag_seconds > kEpsilon || config.encoder_quantization_mm > kEpsilon);
}

double quantizePosition(double value, double quantum_mm) {
    if (quantum_mm <= kEpsilon) {
        return value;
    }

    return std::round(value / quantum_mm) * quantum_mm;
}

double followingErrorThreshold(const MotionRealismConfig& config) {
    if (config.following_error_threshold_mm > kEpsilon) {
        return config.following_error_threshold_mm;
    }
    if (config.encoder_quantization_mm > kEpsilon) {
        return std::max(config.encoder_quantization_mm * 0.5, kDefaultFollowingErrorThresholdMm);
    }

    return kDefaultFollowingErrorThresholdMm;
}

struct LimitResolution {
    double target_position_mm{0.0};
    bool positive_soft_limit{false};
    bool negative_soft_limit{false};
};

struct AxisRuntime {
    AxisState state{};
    AxisCommand pending_command{};
    AxisCommand active_command{};
    bool has_pending_command{false};
    bool has_active_command{false};
    bool soft_limits_configured{false};
    bool home_completion_pending{false};
    double command_position_mm{0.0};
    double command_velocity_mm_per_s{0.0};
    double feedback_position_unquantized_mm{0.0};
};

class InMemoryVirtualAxisGroup final : public VirtualAxisGroup {
public:
    InMemoryVirtualAxisGroup(std::vector<std::string> axis_names, MotionRealismConfig motion_realism)
        : motion_realism_(std::move(motion_realism)) {
        setAxes(axis_names);
    }

    void setAxes(const std::vector<std::string>& axis_names) override {
        axes_.clear();
        axis_indices_.clear();

        for (const auto& axis_name : axis_names) {
            if (axis_indices_.find(axis_name) != axis_indices_.end()) {
                continue;
            }

            AxisRuntime runtime;
            runtime.state.axis = axis_name;
            synchronizeRuntimeFromState(runtime);
            axis_indices_.emplace(axis_name, axes_.size());
            axes_.push_back(runtime);
        }
    }

    void applyCommands(const std::vector<AxisCommand>& commands) override {
        for (const auto& command : commands) {
            AxisRuntime& runtime = ensureAxis(command.axis);
            if ((runtime.has_pending_command && commandsEquivalent(runtime.pending_command, command)) ||
                (runtime.has_active_command && commandsEquivalent(runtime.active_command, command))) {
                continue;
            }

            runtime.pending_command = command;
            runtime.has_pending_command = true;
        }
    }

    bool hasAxis(const std::string& axis_name) const override {
        return axis_indices_.find(axis_name) != axis_indices_.end();
    }

    AxisState readAxis(const std::string& axis_name) const override {
        const AxisRuntime* runtime = findAxis(axis_name);
        if (runtime == nullptr) {
            return AxisState{axis_name};
        }

        return runtime->state;
    }

    void writeAxisState(const AxisState& state) override {
        AxisRuntime& runtime = ensureAxis(state.axis);
        runtime.state = state;
        runtime.soft_limits_configured = runtime.soft_limits_configured ||
            state.soft_limit_negative_mm < state.soft_limit_positive_mm ||
            hasMeaningfulVelocity(state.soft_limit_negative_mm) ||
            hasMeaningfulVelocity(state.soft_limit_positive_mm);
        synchronizeRuntimeFromState(runtime);

        if (!state.running && state.done) {
            runtime.has_pending_command = false;
            runtime.has_active_command = false;
        }
    }

    void advance(const TickInfo& tick) override {
        const double dt = tickSeconds(tick);
        for (auto& runtime : axes_) {
            const double previous_feedback_position = runtime.state.position_mm;
            runtime.state.index_signal = false;

            if (runtime.has_pending_command) {
                runtime.active_command = runtime.pending_command;
                runtime.has_active_command = true;
                runtime.has_pending_command = false;
            }

            if (!runtime.has_active_command) {
                runtime.command_velocity_mm_per_s = 0.0;
                updateObservedMotion(runtime, dt);
                finalizeHomeIfSettled(runtime);
                refreshDerivedSignals(runtime, previous_feedback_position);
                continue;
            }

            if (!runtime.state.enabled) {
                failAxis(runtime, kAxisErrorDisabled);
                refreshDerivedSignals(runtime, previous_feedback_position);
                continue;
            }

            const AxisCommand command = runtime.active_command;
            if (command.stop_requested) {
                stopAxis(runtime);
                refreshDerivedSignals(runtime, previous_feedback_position);
                continue;
            }

            if (command.home_requested) {
                runtime.home_completion_pending = true;
                advanceTowardTarget(
                    runtime,
                    kHomePositionMm,
                    commandVelocity(runtime.state, command),
                    dt,
                    true,
                    previous_feedback_position);
                continue;
            }

            const auto resolved_limit = resolveSoftLimit(runtime, command.target_position_mm);
            advanceTowardTarget(
                runtime,
                resolved_limit.target_position_mm,
                commandVelocity(runtime.state, command),
                dt,
                false,
                previous_feedback_position,
                resolved_limit.positive_soft_limit,
                resolved_limit.negative_soft_limit);
        }
    }

    std::vector<AxisState> snapshot() const override {
        std::vector<AxisState> states;
        states.reserve(axes_.size());
        for (const auto& axis : axes_) {
            states.push_back(axis.state);
        }

        return states;
    }

private:
    const AxisRuntime* findAxis(const std::string& axis_name) const {
        const auto it = axis_indices_.find(axis_name);
        if (it == axis_indices_.end()) {
            return nullptr;
        }

        return &axes_[it->second];
    }

    AxisRuntime& ensureAxis(const std::string& axis_name) {
        const auto it = axis_indices_.find(axis_name);
        if (it != axis_indices_.end()) {
            return axes_[it->second];
        }

        AxisRuntime runtime;
        runtime.state.axis = axis_name;
        synchronizeRuntimeFromState(runtime);
        axis_indices_.emplace(axis_name, axes_.size());
        axes_.push_back(runtime);
        return axes_.back();
    }

    void synchronizeRuntimeFromState(AxisRuntime& runtime) {
        runtime.command_position_mm = runtime.state.position_mm;
        runtime.command_velocity_mm_per_s = runtime.state.velocity_mm_per_s;
        runtime.feedback_position_unquantized_mm = runtime.state.position_mm;
        runtime.home_completion_pending = false;
        runtime.state.command_position_mm = runtime.command_position_mm;
        runtime.state.command_velocity_mm_per_s = runtime.command_velocity_mm_per_s;
        runtime.state.following_error_mm = 0.0;
        runtime.state.following_error_active = false;
        runtime.state.encoder_quantization_mm =
            motionRealismEnabled(motion_realism_) ? motion_realism_.encoder_quantization_mm : 0.0;
    }

    LimitResolution resolveSoftLimit(const AxisRuntime& runtime, double target_position_mm) const {
        LimitResolution resolution;
        resolution.target_position_mm = target_position_mm;

        if (!runtime.soft_limits_configured) {
            return resolution;
        }

        if (target_position_mm > runtime.state.soft_limit_positive_mm) {
            resolution.target_position_mm = runtime.state.soft_limit_positive_mm;
            resolution.positive_soft_limit = true;
        } else if (target_position_mm < runtime.state.soft_limit_negative_mm) {
            resolution.target_position_mm = runtime.state.soft_limit_negative_mm;
            resolution.negative_soft_limit = true;
        }

        return resolution;
    }

    bool positiveHardLimitActive(const AxisRuntime& runtime, double direction) const {
        return direction > kEpsilon &&
            runtime.state.positive_hard_limit_enabled &&
            runtime.state.positive_hard_limit;
    }

    bool negativeHardLimitActive(const AxisRuntime& runtime, double direction) const {
        return direction < -kEpsilon &&
            runtime.state.negative_hard_limit_enabled &&
            runtime.state.negative_hard_limit;
    }

    double servoResponseRatio(double dt) const {
        if (!motionRealismEnabled(motion_realism_) || dt <= 0.0 || motion_realism_.servo_lag_seconds <= kEpsilon) {
            return 1.0;
        }

        const auto response = 1.0 - std::exp(-dt / motion_realism_.servo_lag_seconds);
        return std::clamp(response, 0.0, 1.0);
    }

    void updateObservedMotion(AxisRuntime& runtime, double dt) {
        const double previous_feedback_position = runtime.state.position_mm;
        runtime.state.command_position_mm = runtime.command_position_mm;
        runtime.state.command_velocity_mm_per_s = runtime.command_velocity_mm_per_s;

        if (!motionRealismEnabled(motion_realism_)) {
            runtime.feedback_position_unquantized_mm = runtime.command_position_mm;
            runtime.state.position_mm = runtime.command_position_mm;
            runtime.state.velocity_mm_per_s = runtime.command_velocity_mm_per_s;
            runtime.state.following_error_mm = 0.0;
            runtime.state.following_error_active = false;
            runtime.state.encoder_quantization_mm = 0.0;
            runtime.state.running = runtime.has_active_command;
            runtime.state.done = !runtime.state.running;
            return;
        }

        runtime.feedback_position_unquantized_mm +=
            (runtime.command_position_mm - runtime.feedback_position_unquantized_mm) * servoResponseRatio(dt);

        const double encoder_quantization_mm = std::max(0.0, motion_realism_.encoder_quantization_mm);
        const double quantized_feedback_position =
            quantizePosition(runtime.feedback_position_unquantized_mm, encoder_quantization_mm);
        const double feedback_velocity = dt > 0.0
            ? (quantized_feedback_position - previous_feedback_position) / dt
            : 0.0;
        const double error_threshold = followingErrorThreshold(motion_realism_);

        runtime.state.position_mm = quantized_feedback_position;
        runtime.state.velocity_mm_per_s = feedback_velocity;
        runtime.state.following_error_mm = runtime.command_position_mm - runtime.state.position_mm;
        runtime.state.following_error_active =
            std::abs(runtime.state.following_error_mm) > error_threshold + kEpsilon;
        runtime.state.encoder_quantization_mm = encoder_quantization_mm;

        const bool feedback_settling =
            runtime.state.following_error_active || std::abs(feedback_velocity) > kVelocityEpsilon;
        runtime.state.running = runtime.has_active_command || feedback_settling;
        runtime.state.done = !runtime.state.running;
    }

    void finalizeHomeIfSettled(AxisRuntime& runtime) {
        if (!runtime.home_completion_pending) {
            return;
        }

        if (runtime.state.has_error || runtime.state.servo_alarm) {
            runtime.home_completion_pending = false;
            return;
        }

        if (runtime.state.running) {
            runtime.state.homed = false;
            return;
        }

        if (std::abs(runtime.state.position_mm - kHomePositionMm) <= followingErrorThreshold(motion_realism_) + kEpsilon) {
            runtime.state.homed = true;
            runtime.home_completion_pending = false;
            return;
        }

        runtime.state.homed = false;
    }

    void advanceTowardTarget(AxisRuntime& runtime,
                             double target_position_mm,
                             double speed_mm_per_s,
                             double dt,
                             bool homing,
                             double previous_feedback_position,
                             bool positive_soft_limit_requested = false,
                             bool negative_soft_limit_requested = false) {
        const double remaining = target_position_mm - runtime.command_position_mm;
        const double direction = nearlyEqual(remaining, 0.0) ? 0.0 : (remaining > 0.0 ? 1.0 : -1.0);

        if (positiveHardLimitActive(runtime, direction)) {
            failAxis(runtime, kAxisErrorPositiveHardLimit);
            refreshDerivedSignals(runtime, previous_feedback_position);
            return;
        }
        if (negativeHardLimitActive(runtime, direction)) {
            failAxis(runtime, kAxisErrorNegativeHardLimit);
            refreshDerivedSignals(runtime, previous_feedback_position);
            return;
        }

        if (direction == 0.0 || dt <= 0.0 || speed_mm_per_s <= kEpsilon) {
            runtime.command_position_mm = target_position_mm;
            runtime.command_velocity_mm_per_s = 0.0;
            runtime.has_active_command = false;
            updateObservedMotion(runtime, dt);

            if (homing) {
                runtime.state.homed = false;
            }

            if (positive_soft_limit_requested) {
                setLimitFault(runtime, kAxisErrorPositiveSoftLimit);
            } else if (negative_soft_limit_requested) {
                setLimitFault(runtime, kAxisErrorNegativeSoftLimit);
            }

            finalizeHomeIfSettled(runtime);
            refreshDerivedSignals(runtime, previous_feedback_position);
            return;
        }

        const double travel = speed_mm_per_s * dt;
        if (travel + kEpsilon >= std::abs(remaining)) {
            runtime.command_position_mm = target_position_mm;
            runtime.command_velocity_mm_per_s = 0.0;
            runtime.has_active_command = false;
        } else {
            runtime.command_position_mm += direction * travel;
            runtime.command_velocity_mm_per_s = direction * speed_mm_per_s;
        }

        updateObservedMotion(runtime, dt);
        if (homing) {
            runtime.state.homed = false;
        }

        if (!runtime.has_active_command) {
            if (positive_soft_limit_requested) {
                setLimitFault(runtime, kAxisErrorPositiveSoftLimit);
            } else if (negative_soft_limit_requested) {
                setLimitFault(runtime, kAxisErrorNegativeSoftLimit);
            }
        }

        finalizeHomeIfSettled(runtime);
        refreshDerivedSignals(runtime, previous_feedback_position);
    }

    void refreshDerivedSignals(AxisRuntime& runtime, double previous_feedback_position) {
        runtime.state.home_signal = nearlyEqual(runtime.state.position_mm, kHomePositionMm);
        runtime.state.index_signal = crossedIndex(previous_feedback_position, runtime.state.position_mm);

        if (runtime.soft_limits_configured) {
            runtime.state.positive_soft_limit =
                runtime.state.position_mm >= runtime.state.soft_limit_positive_mm - kEpsilon;
            runtime.state.negative_soft_limit =
                runtime.state.position_mm <= runtime.state.soft_limit_negative_mm + kEpsilon;
        } else {
            runtime.state.positive_soft_limit = false;
            runtime.state.negative_soft_limit = false;
        }
    }

    bool crossedIndex(double previous_position, double current_position) const {
        if (nearlyEqual(previous_position, current_position)) {
            return false;
        }

        return (previous_position > kHomePositionMm && current_position <= kHomePositionMm + kEpsilon) ||
            (previous_position < kHomePositionMm && current_position >= kHomePositionMm - kEpsilon) ||
            nearlyEqual(current_position, kHomePositionMm);
    }

    void snapMotionToFeedback(AxisRuntime& runtime) {
        runtime.command_position_mm = runtime.state.position_mm;
        runtime.command_velocity_mm_per_s = 0.0;
        runtime.feedback_position_unquantized_mm = runtime.state.position_mm;
        runtime.state.command_position_mm = runtime.command_position_mm;
        runtime.state.command_velocity_mm_per_s = 0.0;
        runtime.state.velocity_mm_per_s = 0.0;
        runtime.state.following_error_mm = 0.0;
        runtime.state.following_error_active = false;
        runtime.state.encoder_quantization_mm =
            motionRealismEnabled(motion_realism_) ? motion_realism_.encoder_quantization_mm : 0.0;
        runtime.state.running = false;
        runtime.state.done = true;
        runtime.has_active_command = false;
        runtime.has_pending_command = false;
        runtime.home_completion_pending = false;
    }

    void stopAxis(AxisRuntime& runtime) {
        snapMotionToFeedback(runtime);
    }

    void failAxis(AxisRuntime& runtime, int error_code) {
        snapMotionToFeedback(runtime);
        runtime.state.has_error = true;
        runtime.state.error_code = error_code;
    }

    void setLimitFault(AxisRuntime& runtime, int error_code) {
        runtime.state.has_error = true;
        runtime.state.error_code = error_code;
    }

    MotionRealismConfig motion_realism_{};
    std::vector<AxisRuntime> axes_{};
    std::unordered_map<std::string, std::size_t> axis_indices_{};
};

}  // namespace

std::unique_ptr<VirtualAxisGroup> createInMemoryVirtualAxisGroup(
    const std::vector<std::string>& axis_names,
    MotionRealismConfig motion_realism) {
    return std::make_unique<InMemoryVirtualAxisGroup>(axis_names, std::move(motion_realism));
}

}  // namespace sim::scheme_c
