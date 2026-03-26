#include "JogController.h"
#include "domain/safety/bridges/MotionCoreInterlockBridge.h"

#define MODULE_NAME "JogController"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"

namespace Siligen::Domain::Motion::DomainServices {

using Siligen::Shared::Types::IsValid;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::ToIndex;

// 常量定义
constexpr float kMinVelocity = 0.1f;    // 最小速度 (mm/s)
constexpr float kMaxVelocity = 500.0f;  // 最大速度 (mm/s)
constexpr float kMinDistance = 0.01f;   // 最小距离 (mm)
constexpr float kMaxDistance = 100.0f;  // 最大距离 (mm)
constexpr int16_t kMinAxis = 0;         // 最小轴号
constexpr int16_t kMaxAxis = 1;         // 最大轴号

constexpr int16_t NormalizeDirection(int16_t direction) noexcept {
    return (direction >= 0) ? 1 : -1;
}

JogController::JogController(std::shared_ptr<IJogControlPort> jog_port,
                              std::shared_ptr<IMotionStatePort> state_port) noexcept
    : jog_port_(std::move(jog_port)), state_port_(std::move(state_port)) {}

Result<void> JogController::StartContinuousJog(LogicalAxisId axis, int16_t direction, float velocity) noexcept {
    if (!jog_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Jog control port not initialized", "JogController"));
    }

    const int16_t normalized_direction = NormalizeDirection(direction);

    // 记录调试日志
    SILIGEN_LOG_DEBUG_FMT_HELPER("[JogController] StartContinuousJog: axis=%d, direction=%d, velocity=%.2f",
            ToIndex(axis), normalized_direction, velocity);

    // 1. 参数验证
    auto validation_result = ValidateJogParameters(axis, velocity);
    if (validation_result.IsError()) {
        return validation_result;
    }

    // 2. 安全互锁检查
    auto safety_result = CheckSafetyInterlocks(axis, normalized_direction);
    if (safety_result.IsError()) {
        SILIGEN_LOG_ERROR("[JogController] Safety interlock check FAILED");
        return safety_result;
    }
    SILIGEN_LOG_DEBUG("[JogController] Safety check passed, calling jog_port_->StartJog");

    // 3. 调用Port接口
    auto result = jog_port_->StartJog(axis, normalized_direction, velocity);
    SILIGEN_LOG_DEBUG_FMT_HELPER("[JogController] jog_port_->StartJog returned, success=%d", result.IsSuccess());
    return result;
}

Result<void> JogController::StartStepJog(LogicalAxisId axis, int16_t direction, float distance, float velocity) noexcept {
    if (!jog_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Jog control port not initialized", "JogController"));
    }

    const int16_t normalized_direction = NormalizeDirection(direction);

    // 1. 参数验证
    auto validation_result = ValidateJogParameters(axis, velocity);
    if (validation_result.IsError()) {
        return validation_result;
    }

    // 验证距离
    auto distance_result = ValidateDistance(distance);
    if (distance_result.IsError()) {
        return distance_result;
    }

    // 2. 安全互锁检查
    auto safety_result = CheckSafetyInterlocks(axis, normalized_direction);
    if (safety_result.IsError()) {
        return safety_result;
    }

    // 3. 调用Port接口
    return jog_port_->StartJogStep(axis, normalized_direction, distance, velocity);
}

Result<void> JogController::StopJog(LogicalAxisId axis) noexcept {
    if (!jog_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Jog control port not initialized", "JogController"));
    }

    // 验证轴号
    const int16_t axis_index = ToIndex(axis);
    if (!IsValid(axis) || axis_index < kMinAxis || axis_index > kMaxAxis) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_AXIS, "Axis must be in range [0, 1]", "JogController"));
    }

    // 调用Port接口
    return jog_port_->StopJog(axis);
}

Result<JogState> JogController::GetJogState(LogicalAxisId axis) const noexcept {
    if (!jog_port_) {
        return Result<JogState>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Jog control port not initialized", "JogController"));
    }

    // 验证轴号
    const int16_t axis_index = ToIndex(axis);
    if (!IsValid(axis) || axis_index < kMinAxis || axis_index > kMaxAxis) {
        return Result<JogState>::Failure(
            Error(ErrorCode::INVALID_AXIS, "Axis must be in range [0, 1]", "JogController"));
    }

    // 注意：当前Port接口没有GetJogState方法
    // 这里返回Idle状态作为占位符
    // TODO: 需要在IJogControlPort中添加GetJogState方法
    return Result<JogState>::Success(JogState::Idle);
}

Result<void> JogController::CheckSafetyInterlocks(LogicalAxisId axis, int16_t direction) const noexcept {
    // 1. 检查轴号有效性
    const int16_t axis_index = ToIndex(axis);
    if (!IsValid(axis) || axis_index < kMinAxis || axis_index > kMaxAxis) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_AXIS, "Axis must be in range [0, 1]", "JogController"));
    }

    if (!state_port_) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Motion state port not initialized", "JogController"));
    }

    // 2. 检查急停状态和限位开关（通过IMotionStatePort）
    auto status_result = state_port_->GetAxisStatus(axis);
    if (status_result.IsError()) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_ERROR,
                  "Failed to get axis status: " + status_result.GetError().GetMessage(),
                  "JogController"));
    }

    const auto& status = status_result.Value();
    return Siligen::Domain::Safety::Bridges::CheckAxisForJogWithMotionCore(
        status, direction, "JogController");
}

Result<void> JogController::ValidateJogParameters(LogicalAxisId axis, float velocity) const noexcept {
    // 验证轴号
    const int16_t axis_index = ToIndex(axis);
    if (!IsValid(axis) || axis_index < kMinAxis || axis_index > kMaxAxis) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_AXIS, "Axis must be in range [0, 1]", "JogController"));
    }

    // 验证速度范围
    if (velocity < kMinVelocity || velocity > kMaxVelocity) {
        return Result<void>::Failure(
            Error(ErrorCode::VELOCITY_LIMIT_EXCEEDED,
                  "Velocity out of range",
                  "JogController"));
    }

    return Result<void>::Success();
}

Result<void> JogController::ValidateDistance(float distance) const noexcept {
    // 验证距离范围
    if (distance < kMinDistance || distance > kMaxDistance) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER,
                  "Distance out of range",
                  "JogController"));
    }

    return Result<void>::Success();
}

}  // namespace Siligen::Domain::Motion::DomainServices



