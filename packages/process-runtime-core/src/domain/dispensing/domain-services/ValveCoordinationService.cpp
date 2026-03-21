#include "ValveCoordinationService.h"

#include <algorithm>

namespace Siligen::Domain::Dispensing::DomainServices {

ValveCoordinationService::ValveCoordinationService(std::shared_ptr<IValvePort> valve_port) noexcept
    : valve_port_(std::move(valve_port)) {}

// ============================================================
// 供胶阀控制
// ============================================================

Result<SupplyValveState> ValveCoordinationService::OpenSupplyValve() noexcept {
    auto safety_result = CheckValveSafety(false);
    if (safety_result.IsError()) {
        return Result<SupplyValveState>::Failure(safety_result.GetError());
    }

    if (!valve_port_) {
        return Result<SupplyValveState>::Failure(
            Error(
                ErrorCode::PORT_NOT_INITIALIZED,
                "failure_stage=open_supply;failure_code=PORT_NOT_INITIALIZED;message=valve_port_unavailable",
                "ValveCoordinationService"));
    }

    return valve_port_->OpenSupply();
}

Result<SupplyValveState> ValveCoordinationService::CloseSupplyValve() noexcept {
    if (!valve_port_) {
        return Result<SupplyValveState>::Failure(
            Error(
                ErrorCode::PORT_NOT_INITIALIZED,
                "failure_stage=close_supply;failure_code=PORT_NOT_INITIALIZED;message=valve_port_unavailable",
                "ValveCoordinationService"));
    }
    return valve_port_->CloseSupply();
}

Result<SupplyValveStatusDetail> ValveCoordinationService::GetSupplyValveStatus() noexcept {
    if (!valve_port_) {
        return Result<SupplyValveStatusDetail>::Failure(
            Error(
                ErrorCode::PORT_NOT_INITIALIZED,
                "failure_stage=get_supply_status;failure_code=PORT_NOT_INITIALIZED;message=valve_port_unavailable",
                "ValveCoordinationService"));
    }
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
    auto safety_result = CheckValveSafety(true);
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
    auto safety_result = CheckValveSafety(true);
    if (safety_result.IsError()) {
        return Result<DispenserValveState>::Failure(safety_result.GetError());
    }

    // 3. 调用Port接口
    return valve_port_->StartPositionTriggeredDispenser(params);
}

Result<void> ValveCoordinationService::StopDispenser() noexcept {
    if (!valve_port_) {
        return Result<void>::Failure(
            Error(
                ErrorCode::PORT_NOT_INITIALIZED,
                "failure_stage=stop_dispenser;failure_code=PORT_NOT_INITIALIZED;message=valve_port_unavailable",
                "ValveCoordinationService"));
    }
    return valve_port_->StopDispenser();
}

Result<void> ValveCoordinationService::PauseDispenser() noexcept {
    if (!valve_port_) {
        return Result<void>::Failure(
            Error(
                ErrorCode::PORT_NOT_INITIALIZED,
                "failure_stage=pause_dispenser;failure_code=PORT_NOT_INITIALIZED;message=valve_port_unavailable",
                "ValveCoordinationService"));
    }
    return valve_port_->PauseDispenser();
}

Result<void> ValveCoordinationService::ResumeDispenser() noexcept {
    auto safety_result = CheckValveSafety(true);
    if (safety_result.IsError()) {
        return safety_result;
    }

    return valve_port_->ResumeDispenser();
}

Result<DispenserValveState> ValveCoordinationService::GetDispenserStatus() noexcept {
    if (!valve_port_) {
        return Result<DispenserValveState>::Failure(
            Error(
                ErrorCode::PORT_NOT_INITIALIZED,
                "failure_stage=get_dispenser_status;failure_code=PORT_NOT_INITIALIZED;message=valve_port_unavailable",
                "ValveCoordinationService"));
    }
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

Result<void> ValveCoordinationService::CheckValveSafety(bool require_supply_open) const noexcept {
    if (!valve_port_) {
        return Result<void>::Failure(
            Error(
                ErrorCode::PORT_NOT_INITIALIZED,
                "failure_stage=check_valve_safety;failure_code=PORT_NOT_INITIALIZED;message=valve_port_unavailable",
                "ValveCoordinationService"));
    }

    auto dispenser_status_result = valve_port_->GetDispenserStatus();
    if (dispenser_status_result.IsError()) {
        return Result<void>::Failure(
            Error(
                dispenser_status_result.GetError().GetCode(),
                "failure_stage=check_valve_safety_dispenser_status;failure_code=" +
                    std::to_string(static_cast<int>(dispenser_status_result.GetError().GetCode())) +
                    ";message=" + dispenser_status_result.GetError().GetMessage(),
                "ValveCoordinationService"));
    }
    if (dispenser_status_result.Value().HasError()) {
        return Result<void>::Failure(
            Error(
                ErrorCode::HARDWARE_ERROR,
                "failure_stage=check_valve_safety_dispenser_status;failure_code=DISPENSER_ERROR;message=dispenser_status_error",
                "ValveCoordinationService"));
    }

    auto supply_status_result = valve_port_->GetSupplyStatus();
    if (supply_status_result.IsError()) {
        return Result<void>::Failure(
            Error(
                supply_status_result.GetError().GetCode(),
                "failure_stage=check_valve_safety_supply_status;failure_code=" +
                    std::to_string(static_cast<int>(supply_status_result.GetError().GetCode())) +
                    ";message=" + supply_status_result.GetError().GetMessage(),
                "ValveCoordinationService"));
    }

    const auto& supply_status = supply_status_result.Value();
    if (supply_status.HasError()) {
        return Result<void>::Failure(
            Error(
                ErrorCode::HARDWARE_ERROR,
                "failure_stage=check_valve_safety_supply_status;failure_code=SUPPLY_STATUS_ERROR;message=supply_status_error",
                "ValveCoordinationService"));
    }

    if (require_supply_open && supply_status.state != SupplyValveState::Open) {
        return Result<void>::Failure(
            Error(
                ErrorCode::INVALID_STATE,
                "failure_stage=check_valve_safety_supply_open;failure_code=SUPPLY_NOT_OPEN;message=supply_valve_not_open",
                "ValveCoordinationService"));
    }

    return Result<void>::Success();
}

}  // namespace Siligen::Domain::Dispensing::DomainServices
