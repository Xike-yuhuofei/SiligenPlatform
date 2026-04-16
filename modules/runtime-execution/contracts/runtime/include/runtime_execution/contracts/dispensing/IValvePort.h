#pragma once

#include "shared/types/Error.h"
#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace Siligen::Domain::Dispensing::Ports {

using Siligen::Shared::Types::float32;
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

struct PathTriggerEvent {
    uint32 sequence_index = 0;
    float32 path_position_mm = 0.0f;
    long x_position_pulse = 0;
    long y_position_pulse = 0;

    bool operator==(const PathTriggerEvent& other) const noexcept {
        return sequence_index == other.sequence_index &&
               path_position_mm == other.path_position_mm &&
               x_position_pulse == other.x_position_pulse &&
               y_position_pulse == other.y_position_pulse;
    }
};

struct PositionTriggeredDispenserParams {
    static constexpr size_t kMaxTriggerEvents = 100000;
    std::vector<PathTriggerEvent> trigger_events;
    uint32 pulse_width_ms = 100;
    short start_level = 0;
    short coordinate_system = 1;
    long position_tolerance_pulse = 0;

    bool IsValid() const noexcept {
        if (trigger_events.empty() ||
            trigger_events.size() > kMaxTriggerEvents ||
            pulse_width_ms < 10 ||
            pulse_width_ms > 5000 ||
            (start_level != 0 && start_level != 1) ||
            coordinate_system <= 0 ||
            position_tolerance_pulse < 0) {
            return false;
        }

        float32 previous_path_position_mm = -1.0f;
        uint32 previous_sequence_index = 0;
        bool has_previous = false;
        for (const auto& event : trigger_events) {
            if (event.path_position_mm < 0.0f) {
                return false;
            }
            if (has_previous) {
                if (event.sequence_index <= previous_sequence_index) {
                    return false;
                }
                if (event.path_position_mm + 1e-6f < previous_path_position_mm) {
                    return false;
                }
            }
            previous_sequence_index = event.sequence_index;
            previous_path_position_mm = event.path_position_mm;
            has_previous = true;
        }

        return true &&
               pulse_width_ms >= 10 &&
               pulse_width_ms <= 5000 &&
               (start_level == 0 || start_level == 1);
    }

    std::string GetValidationError() const {
        if (trigger_events.empty()) {
            return "路径触发事件不能为空";
        }
        if (trigger_events.size() > kMaxTriggerEvents) {
            return "路径触发事件数量不能超过" + std::to_string(kMaxTriggerEvents);
        }
        for (size_t index = 0; index < trigger_events.size(); ++index) {
            const auto& event = trigger_events[index];
            if (event.path_position_mm < 0.0f) {
                return "路径触发事件的 path_position_mm 不能为负数";
            }
            if (index > 0) {
                if (event.sequence_index <= trigger_events[index - 1].sequence_index) {
                    return "路径触发事件 sequence_index 必须严格递增";
                }
                if (event.path_position_mm + 1e-6f < trigger_events[index - 1].path_position_mm) {
                    return "路径触发事件 path_position_mm 必须单调不减";
                }
            }
        }
        if (pulse_width_ms < 10 || pulse_width_ms > 5000) {
            return "脉冲宽度必须在10-5000ms范围内";
        }
        if (start_level != 0 && start_level != 1) {
            return "起始电平必须为0（低电平）或1（高电平）";
        }
        if (coordinate_system <= 0) {
            return "坐标系编号必须大于0";
        }
        if (position_tolerance_pulse < 0) {
            return "路径触发容差脉冲不能为负数";
        }
        return "";
    }

    bool operator==(const PositionTriggeredDispenserParams& other) const noexcept {
        return trigger_events == other.trigger_events &&
               pulse_width_ms == other.pulse_width_ms &&
               start_level == other.start_level &&
               coordinate_system == other.coordinate_system &&
               position_tolerance_pulse == other.position_tolerance_pulse;
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
