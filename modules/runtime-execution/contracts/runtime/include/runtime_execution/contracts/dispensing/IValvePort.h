#pragma once

#include "shared/types/Error.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace Siligen::Domain::Dispensing::Ports {

using Siligen::Shared::Types::LogicalAxisId;

struct DispenserValveParams {
    uint32 count = 1;
    uint32 intervalMs = 1000;
    uint32 durationMs = 100;

    bool IsValid() const noexcept {
        return (count >= 1 && count <= 1000) &&
               (intervalMs >= 10 && intervalMs <= 60000) &&
               (durationMs >= 10 && durationMs <= 5000);
    }

    std::string GetValidationError() const {
        if (count < 1 || count > 1000) {
            return "点胶次数必须在1-1000范围内";
        }
        if (intervalMs < 10 || intervalMs > 60000) {
            return "点胶间隔必须在10-60000ms范围内";
        }
        if (durationMs < 10 || durationMs > 5000) {
            return "单次持续时间必须在10-5000ms范围内";
        }
        return "";
    }

    bool operator==(const DispenserValveParams& other) const noexcept {
        return count == other.count &&
               intervalMs == other.intervalMs &&
               durationMs == other.durationMs;
    }
};

struct PositionTriggeredDispenserParams {
    static constexpr size_t kMaxTriggerPositions = 100000;
    std::vector<long> trigger_positions;
    LogicalAxisId axis = LogicalAxisId::X;
    uint32 pulse_width_ms = 100;
    short start_level = 0;

    bool IsValid() const noexcept {
        return !trigger_positions.empty() &&
               trigger_positions.size() <= kMaxTriggerPositions &&
               Siligen::Shared::Types::IsValid(axis) &&
               pulse_width_ms >= 10 &&
               pulse_width_ms <= 5000 &&
               (start_level == 0 || start_level == 1);
    }

    std::string GetValidationError() const {
        if (trigger_positions.empty()) {
            return "触发位置数组不能为空";
        }
        if (trigger_positions.size() > kMaxTriggerPositions) {
            return "触发位置数组长度不能超过" + std::to_string(kMaxTriggerPositions);
        }
        if (!Siligen::Shared::Types::IsValid(axis)) {
            return "轴号必须为有效逻辑轴 (0-based: X=0, Y=1, Z=2, U=3)";
        }
        if (pulse_width_ms < 10 || pulse_width_ms > 5000) {
            return "脉冲宽度必须在10-5000ms范围内";
        }
        if (start_level != 0 && start_level != 1) {
            return "起始电平必须为0（低电平）或1（高电平）";
        }
        return "";
    }

    bool operator==(const PositionTriggeredDispenserParams& other) const noexcept {
        return trigger_positions == other.trigger_positions &&
               axis == other.axis &&
               pulse_width_ms == other.pulse_width_ms &&
               start_level == other.start_level;
    }
};

enum class DispenserValveStatus {
    Idle,
    Running,
    Paused,
    Stopped,
    Error
};

struct DispenserValveState {
    DispenserValveStatus status = DispenserValveStatus::Idle;
    uint32 completedCount = 0;
    uint32 totalCount = 0;
    uint32 remainingCount = 0;
    float progress = 0.0f;
    std::optional<uint64> startTime;
    std::string errorMessage;

    bool IsRunning() const noexcept {
        return status == DispenserValveStatus::Running;
    }

    bool IsCompleted() const noexcept {
        return status == DispenserValveStatus::Idle && completedCount == totalCount && totalCount > 0;
    }

    bool HasError() const noexcept {
        return status == DispenserValveStatus::Error || !errorMessage.empty();
    }
};

enum class SupplyValveState {
    Open,
    Closed,
    Error
};

struct SupplyValveStatusDetail {
    SupplyValveState state = SupplyValveState::Closed;
    std::optional<uint64> lastChangeTime;
    std::string errorMessage;

    bool IsOpen() const noexcept {
        return state == SupplyValveState::Open;
    }

    bool HasError() const noexcept {
        return state == SupplyValveState::Error || !errorMessage.empty();
    }
};

class IValvePort {
   public:
    virtual ~IValvePort() = default;

    virtual Shared::Types::Result<DispenserValveState> StartDispenser(const DispenserValveParams& params) noexcept = 0;
    virtual Shared::Types::Result<DispenserValveState> OpenDispenser() noexcept = 0;
    virtual Shared::Types::Result<void> CloseDispenser() noexcept = 0;
    virtual Shared::Types::Result<DispenserValveState> StartPositionTriggeredDispenser(
        const PositionTriggeredDispenserParams& params) noexcept = 0;
    virtual Shared::Types::Result<void> StopDispenser() noexcept = 0;
    virtual Shared::Types::Result<void> PauseDispenser() noexcept = 0;
    virtual Shared::Types::Result<void> ResumeDispenser() noexcept = 0;
    virtual Shared::Types::Result<DispenserValveState> GetDispenserStatus() noexcept = 0;
    virtual Shared::Types::Result<SupplyValveState> OpenSupply() noexcept = 0;
    virtual Shared::Types::Result<SupplyValveState> CloseSupply() noexcept = 0;
    virtual Shared::Types::Result<SupplyValveStatusDetail> GetSupplyStatus() noexcept = 0;
};

}  // namespace Siligen::Domain::Dispensing::Ports
