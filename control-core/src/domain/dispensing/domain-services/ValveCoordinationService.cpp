#include "ValveCoordinationService.h"

#include <algorithm>

namespace Siligen::Domain::Dispensing::DomainServices {

ValveCoordinationService::ValveCoordinationService(std::shared_ptr<IValvePort> valve_port) noexcept
    : valve_port_(std::move(valve_port)) {}

// ============================================================
// 供胶阀控制
// ============================================================

Result<SupplyValveState> ValveCoordinationService::OpenSupplyValve() noexcept {
    // 安全检查
    auto safety_result = CheckValveSafety();
    if (safety_result.IsError()) {
        return Result<SupplyValveState>::Failure(safety_result.GetError());
    }

    // 调用Port接口
    return valve_port_->OpenSupply();
}

Result<SupplyValveState> ValveCoordinationService::CloseSupplyValve() noexcept {
    // 调用Port接口（关闭操作不需要安全检查）
    return valve_port_->CloseSupply();
}

Result<SupplyValveStatusDetail> ValveCoordinationService::GetSupplyValveStatus() noexcept {
    // 调用Port接口
    return valve_port_->GetSupplyStatus();
}

// ============================================================
// 点胶阀控制
// ============================================================

Result<DispenserValveState> ValveCoordinationService::StartDispenser(const DispenserValveParams& params) noexcept {
    // 1. 参数验证
    auto validation_result = ValidateDispenserParameters(params);
    if (validation_result.IsError()) {
        return Result<DispenserValveState>::Failure(validation_result.GetError());
    }

    // 2. 安全检查
    auto safety_result = CheckValveSafety();
    if (safety_result.IsError()) {
        return Result<DispenserValveState>::Failure(safety_result.GetError());
    }

    // 3. 调用Port接口
    return valve_port_->StartDispenser(params);
}

Result<DispenserValveState> ValveCoordinationService::StartPositionTriggeredDispenser(
    const PositionTriggeredDispenserParams& params) noexcept {
    // 1. 参数验证
    auto validation_result = ValidatePositionTriggeredParameters(params);
    if (validation_result.IsError()) {
        return Result<DispenserValveState>::Failure(validation_result.GetError());
    }

    // 2. 安全检查
    auto safety_result = CheckValveSafety();
    if (safety_result.IsError()) {
        return Result<DispenserValveState>::Failure(safety_result.GetError());
    }

    // 3. 调用Port接口
    return valve_port_->StartPositionTriggeredDispenser(params);
}

Result<void> ValveCoordinationService::StopDispenser() noexcept {
    // 调用Port接口
    return valve_port_->StopDispenser();
}

Result<void> ValveCoordinationService::PauseDispenser() noexcept {
    // 调用Port接口
    return valve_port_->PauseDispenser();
}

Result<void> ValveCoordinationService::ResumeDispenser() noexcept {
    // 安全检查
    auto safety_result = CheckValveSafety();
    if (safety_result.IsError()) {
        return safety_result;
    }

    // 调用Port接口
    return valve_port_->ResumeDispenser();
}

Result<DispenserValveState> ValveCoordinationService::GetDispenserStatus() noexcept {
    // 调用Port接口
    return valve_port_->GetDispenserStatus();
}

// ============================================================
// 私有方法
// ============================================================

Result<void> ValveCoordinationService::ValidateDispenserParameters(const DispenserValveParams& params) const noexcept {
    // 使用参数结构体的验证方法
    if (!params.IsValid()) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, params.GetValidationError(), "ValveCoordinationService"));
    }

    return Result<void>::Success();
}

Result<void> ValveCoordinationService::ValidatePositionTriggeredParameters(
    const PositionTriggeredDispenserParams& params) const noexcept {
    // 使用参数结构体的验证方法
    if (!params.IsValid()) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, params.GetValidationError(), "ValveCoordinationService"));
    }

    return Result<void>::Success();
}

Result<DispenserValveParams> ValveCoordinationService::ApplySafetyBoundary(
    const DispenserValveParams& params,
    const SafetyBoundary& safety) const noexcept {
    uint32 required_interval = 0;
    const auto computed = safety.duration_ms + safety.valve_response_ms + safety.margin_ms;
    if (computed > 0) {
        required_interval = static_cast<uint32>(computed);
    }
    if (safety.min_interval_ms > 0) {
        required_interval = std::max(required_interval, static_cast<uint32>(safety.min_interval_ms));
    }

    if (required_interval == 0 || params.intervalMs >= required_interval) {
        return Result<DispenserValveParams>::Success(params);
    }

    if (!safety.downgrade_on_violation) {
        return Result<DispenserValveParams>::Failure(
            Error(ErrorCode::INVALID_PARAMETER,
                  "点胶间隔小于阀响应安全边界",
                  "ValveCoordinationService"));
    }

    DispenserValveParams adjusted = params;
    adjusted.intervalMs = required_interval;
    if (!adjusted.IsValid()) {
        return Result<DispenserValveParams>::Failure(
            Error(ErrorCode::INVALID_PARAMETER,
                  "安全降级后的点胶参数无效",
                  "ValveCoordinationService"));
    }

    return Result<DispenserValveParams>::Success(adjusted);
}

Result<void> ValveCoordinationService::CheckValveSafety() const noexcept {
    // TODO: 实现安全检查
    // 1. 检查急停状态
    // 2. 检查硬件连接状态
    // 3. 检查气压状态
    // 4. 检查供胶阀状态（点胶前必须打开供胶阀）

    // 当前返回成功，实际实现需要调用IIOControlPort或IHardwareTestPort
    return Result<void>::Success();
}

}  // namespace Siligen::Domain::Dispensing::DomainServices
