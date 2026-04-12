#include "PurgeDispenserProcess.h"

#include <chrono>
#include <optional>
#include <thread>
#include <utility>

namespace Siligen::Domain::Dispensing::DomainServices {

namespace {
constexpr uint32 kMinPollIntervalMs = 10;
constexpr uint32 kMaxPollIntervalMs = 1000;
constexpr uint32 kMinTimeoutMs = 100;
constexpr uint32 kMaxTimeoutMs = 600000;
}  // namespace

bool PurgeDispenserRequest::Validate() const noexcept {
    if (manage_supply && supply_stabilization_ms > 0 &&
        !SupplyStabilizationPolicy::IsOverrideValid(supply_stabilization_ms)) {
        return false;
    }

    if (wait_for_completion) {
        if (poll_interval_ms < kMinPollIntervalMs || poll_interval_ms > kMaxPollIntervalMs) {
            return false;
        }
        if (timeout_ms < kMinTimeoutMs || timeout_ms > kMaxTimeoutMs) {
            return false;
        }
    }

    return true;
}

std::string PurgeDispenserRequest::GetValidationError() const noexcept {
    if (manage_supply && supply_stabilization_ms > 0 &&
        !SupplyStabilizationPolicy::IsOverrideValid(supply_stabilization_ms)) {
        return "供胶阀稳定时间必须在0-5000ms范围内";
    }

    if (wait_for_completion) {
        if (poll_interval_ms < kMinPollIntervalMs || poll_interval_ms > kMaxPollIntervalMs) {
            return "轮询间隔必须在10-1000ms范围内";
        }
        if (timeout_ms < kMinTimeoutMs || timeout_ms > kMaxTimeoutMs) {
            return "超时时间必须在100-600000ms范围内";
        }
    }

    return "";
}

PurgeDispenserProcess::PurgeDispenserProcess(
    std::shared_ptr<IValvePort> valve_port,
    std::shared_ptr<IConfigurationPort> config_port) noexcept
    : valve_port_(std::move(valve_port)), config_port_(std::move(config_port)) {}

Result<PurgeDispenserResponse> PurgeDispenserProcess::Execute(const PurgeDispenserRequest& request) noexcept {
    if (!request.Validate()) {
        return Result<PurgeDispenserResponse>::Failure(
            Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::INVALID_PARAMETER,
                                          request.GetValidationError()));
    }
    if (!valve_port_) {
        return Result<PurgeDispenserResponse>::Failure(
            Siligen::Shared::Types::Error(
                Siligen::Shared::Types::ErrorCode::PORT_NOT_INITIALIZED,
                "点胶阀端口未初始化"));
    }

    PurgeDispenserResponse response;
    response.supply_managed = request.manage_supply;

    if (request.manage_supply) {
        auto supply_open = valve_port_->OpenSupply();
        if (supply_open.IsError()) {
            return Result<PurgeDispenserResponse>::Failure(supply_open.GetError());
        }
        response.supply_state = supply_open.Value();

        const auto override_ms = request.supply_stabilization_ms > 0
                                     ? std::optional<uint32>(request.supply_stabilization_ms)
                                     : std::nullopt;
        auto stabilization_ms = SupplyStabilizationPolicy::Resolve(config_port_, override_ms);
        if (stabilization_ms.IsError()) {
            return Result<PurgeDispenserResponse>::Failure(stabilization_ms.GetError());
        }
        if (stabilization_ms.Value() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(stabilization_ms.Value()));
        }
    }

    auto start_result = valve_port_->OpenDispenser();
    if (start_result.IsError()) {
        if (request.manage_supply) {
            valve_port_->CloseSupply();
        }
        return Result<PurgeDispenserResponse>::Failure(start_result.GetError());
    }
    response.dispenser_state = start_result.Value();

    if (!request.wait_for_completion) {
        response.completed = false;
        return Result<PurgeDispenserResponse>::Success(response);
    }

    auto wait_result = WaitForCompletion(request.timeout_ms, request.poll_interval_ms);
    if (wait_result.IsError()) {
        valve_port_->CloseDispenser();
        if (request.manage_supply) {
            valve_port_->CloseSupply();
        }
        return Result<PurgeDispenserResponse>::Failure(wait_result.GetError());
    }

    response.dispenser_state = wait_result.Value();
    response.completed = response.dispenser_state.status != DispenserValveStatus::Running;

    if (request.manage_supply) {
        auto supply_close = valve_port_->CloseSupply();
        if (supply_close.IsError()) {
            return Result<PurgeDispenserResponse>::Failure(supply_close.GetError());
        }
        response.supply_state = supply_close.Value();
    }

    return Result<PurgeDispenserResponse>::Success(response);
}

Result<DispenserValveState> PurgeDispenserProcess::WaitForCompletion(uint32 timeout_ms,
                                                                    uint32 poll_interval_ms) noexcept {
    auto start_time = std::chrono::steady_clock::now();

    while (true) {
        auto status_result = valve_port_->GetDispenserStatus();
        if (status_result.IsError()) {
            return Result<DispenserValveState>::Failure(status_result.GetError());
        }

        const auto& state = status_result.Value();
        if (state.HasError()) {
            std::string message = state.errorMessage.empty() ? "点胶阀状态异常" : state.errorMessage;
            return Result<DispenserValveState>::Failure(
                Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::HARDWARE_ERROR, message));
        }

        if (state.status == DispenserValveStatus::Paused) {
            return Result<DispenserValveState>::Failure(
                Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::INVALID_STATE, "排胶已被暂停"));
        }

        if (state.status == DispenserValveStatus::Stopped || state.status == DispenserValveStatus::Idle) {
            return Result<DispenserValveState>::Success(state);
        }

        auto elapsed_ms = static_cast<uint32>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time).count());
        if (elapsed_ms > timeout_ms) {
            return Result<DispenserValveState>::Failure(
                Siligen::Shared::Types::Error(Siligen::Shared::Types::ErrorCode::TIMEOUT, "排胶等待超时"));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
    }
}

}  // namespace Siligen::Domain::Dispensing::DomainServices
