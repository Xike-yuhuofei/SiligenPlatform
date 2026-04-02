#include "runtime_execution/application/services/motion/JogController.h"

#include "domain/safety/bridges/MotionCoreInterlockBridge.h"

#define MODULE_NAME "JogController"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"

namespace Siligen::RuntimeExecution::Application::Services::Motion {

using Siligen::Shared::Types::IsValid;
using Siligen::Shared::Types::ToIndex;

namespace {

constexpr float kMinVelocity = 0.1f;
constexpr float kMaxVelocity = 500.0f;
constexpr float kMinDistance = 0.01f;
constexpr float kMaxDistance = 100.0f;
constexpr int16_t kMinAxis = 0;
constexpr int16_t kMaxAxis = 1;

constexpr int16_t NormalizeDirection(int16_t direction) noexcept {
    return direction >= 0 ? 1 : -1;
}

}  // namespace

JogController::JogController(std::shared_ptr<IJogControlPort> jog_port,
                             std::shared_ptr<IMotionStatePort> state_port) noexcept
    : jog_port_(std::move(jog_port)),
      state_port_(std::move(state_port)) {}

Result<void> JogController::StartContinuousJog(LogicalAxisId axis, int16_t direction, float velocity) noexcept {
    if (!jog_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Jog control port not initialized", "JogController"));
    }

    const int16_t normalized_direction = NormalizeDirection(direction);

    SILIGEN_LOG_DEBUG_FMT_HELPER(
        "[JogController] StartContinuousJog: axis=%d, direction=%d, velocity=%.2f",
        ToIndex(axis),
        normalized_direction,
        velocity);

    auto validation_result = ValidateJogParameters(axis, velocity);
    if (validation_result.IsError()) {
        return validation_result;
    }

    auto safety_result = CheckSafetyInterlocks(axis, normalized_direction);
    if (safety_result.IsError()) {
        SILIGEN_LOG_ERROR("[JogController] Safety interlock check FAILED");
        return safety_result;
    }
    SILIGEN_LOG_DEBUG("[JogController] Safety check passed, calling jog_port_->StartJog");

    auto result = jog_port_->StartJog(axis, normalized_direction, velocity);
    SILIGEN_LOG_DEBUG_FMT_HELPER(
        "[JogController] jog_port_->StartJog returned, success=%d",
        result.IsSuccess());
    return result;
}

Result<void> JogController::StartStepJog(
    LogicalAxisId axis,
    int16_t direction,
    float distance,
    float velocity) noexcept {
    if (!jog_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Jog control port not initialized", "JogController"));
    }

    const int16_t normalized_direction = NormalizeDirection(direction);

    auto validation_result = ValidateJogParameters(axis, velocity);
    if (validation_result.IsError()) {
        return validation_result;
    }

    auto distance_result = ValidateDistance(distance);
    if (distance_result.IsError()) {
        return distance_result;
    }

    auto safety_result = CheckSafetyInterlocks(axis, normalized_direction);
    if (safety_result.IsError()) {
        return safety_result;
    }

    return jog_port_->StartJogStep(axis, normalized_direction, distance, velocity);
}

Result<void> JogController::StopJog(LogicalAxisId axis) noexcept {
    if (!jog_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Jog control port not initialized", "JogController"));
    }

    const int16_t axis_index = ToIndex(axis);
    if (!IsValid(axis) || axis_index < kMinAxis || axis_index > kMaxAxis) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_AXIS, "Axis must be in range [0, 1]", "JogController"));
    }

    return jog_port_->StopJog(axis);
}

Result<JogState> JogController::GetJogState(LogicalAxisId axis) const noexcept {
    if (!jog_port_) {
        return Result<JogState>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Jog control port not initialized", "JogController"));
    }

    const int16_t axis_index = ToIndex(axis);
    if (!IsValid(axis) || axis_index < kMinAxis || axis_index > kMaxAxis) {
        return Result<JogState>::Failure(
            Error(ErrorCode::INVALID_AXIS, "Axis must be in range [0, 1]", "JogController"));
    }

    return Result<JogState>::Success(JogState::Idle);
}

Result<void> JogController::CheckSafetyInterlocks(LogicalAxisId axis, int16_t direction) const noexcept {
    const int16_t axis_index = ToIndex(axis);
    if (!IsValid(axis) || axis_index < kMinAxis || axis_index > kMaxAxis) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_AXIS, "Axis must be in range [0, 1]", "JogController"));
    }

    if (!state_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Motion state port not initialized", "JogController"));
    }

    auto status_result = state_port_->GetAxisStatus(axis);
    if (status_result.IsError()) {
        return Result<void>::Failure(
            Error(
                ErrorCode::HARDWARE_ERROR,
                "Failed to get axis status: " + status_result.GetError().GetMessage(),
                "JogController"));
    }

    return Siligen::Domain::Safety::Bridges::CheckAxisForJogWithMotionCore(
        status_result.Value(),
        direction,
        "JogController");
}

Result<void> JogController::ValidateJogParameters(LogicalAxisId axis, float velocity) const noexcept {
    const int16_t axis_index = ToIndex(axis);
    if (!IsValid(axis) || axis_index < kMinAxis || axis_index > kMaxAxis) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_AXIS, "Axis must be in range [0, 1]", "JogController"));
    }

    if (velocity < kMinVelocity || velocity > kMaxVelocity) {
        return Result<void>::Failure(
            Error(ErrorCode::VELOCITY_LIMIT_EXCEEDED, "Velocity out of range", "JogController"));
    }

    return Result<void>::Success();
}

Result<void> JogController::ValidateDistance(float distance) const noexcept {
    if (distance < kMinDistance || distance > kMaxDistance) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Distance out of range", "JogController"));
    }

    return Result<void>::Success();
}

}  // namespace Siligen::RuntimeExecution::Application::Services::Motion
