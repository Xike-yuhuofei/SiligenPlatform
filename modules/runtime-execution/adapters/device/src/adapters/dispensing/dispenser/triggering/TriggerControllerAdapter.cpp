#include "TriggerControllerAdapter.h"

#include "shared/types/Error.h"

#include <algorithm>
#include <unordered_set>

namespace Siligen::Infrastructure::Adapters::Dispensing {

namespace {
constexpr float32 kMinInterval = 0.0001f;

Result<void> EnsurePortReady(const std::shared_ptr<IHardwareTestPort>& port) {
    if (!port) {
        return Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::PORT_NOT_INITIALIZED,
                                 "Trigger hardware port not initialized",
                                 "TriggerControllerAdapter"));
    }
    if (!port->isConnected()) {
        return Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::HARDWARE_NOT_CONNECTED,
                                 "Hardware not connected",
                                 "TriggerControllerAdapter"));
    }
    return Result<void>::Success();
}

TriggerAction ResolveActionForMode(TriggerMode mode, bool is_end_point) {
    if (mode == TriggerMode::RANGE && is_end_point) {
        return TriggerAction::TurnOff;
    }
    return TriggerAction::TurnOn;
}
}  // namespace

TriggerControllerAdapter::TriggerControllerAdapter(std::shared_ptr<IHardwareTestPort> hardware_test_port)
    : hardware_test_port_(std::move(hardware_test_port)) {}

Result<void> TriggerControllerAdapter::ConfigureTrigger(const TriggerConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
    return Result<void>::Success();
}

Result<TriggerConfig> TriggerControllerAdapter::GetTriggerConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return Result<TriggerConfig>::Success(config_);
}

Result<void> TriggerControllerAdapter::SetSingleTrigger(LogicalAxisId axis, float32 position, int32 pulse_width_us) {
    auto port_ready = EnsurePortReady(hardware_test_port_);
    if (port_ready.IsError()) {
        return port_ready;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.mode = TriggerMode::SINGLE_POINT;
        config_.pulse_width_us = pulse_width_us;
    }

    auto clear_result = ClearTrigger(axis);
    if (clear_result.IsError()) {
        return clear_result;
    }

    auto add_result = AddTriggerPoint(axis, position, TriggerAction::TurnOn);
    if (add_result.IsError()) {
        return add_result;
    }

    return EnableTrigger(axis);
}

Result<void> TriggerControllerAdapter::SetContinuousTrigger(LogicalAxisId axis,
                                                            float32 start_pos,
                                                            float32 end_pos,
                                                            float32 interval,
                                                            int32 pulse_width_us) {
    auto port_ready = EnsurePortReady(hardware_test_port_);
    if (port_ready.IsError()) {
        return port_ready;
    }

    if (interval <= kMinInterval || end_pos <= start_pos) {
        return Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_PARAMETER,
                                 "Invalid trigger interval or range",
                                 "TriggerControllerAdapter"));
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.mode = TriggerMode::CONTINUOUS;
        config_.pulse_width_us = pulse_width_us;
    }

    auto clear_result = ClearTrigger(axis);
    if (clear_result.IsError()) {
        return clear_result;
    }

    for (float32 pos = start_pos; pos <= end_pos + kMinInterval; pos += interval) {
        auto add_result = AddTriggerPoint(axis, pos, TriggerAction::TurnOn);
        if (add_result.IsError()) {
            return add_result;
        }
    }

    return EnableTrigger(axis);
}

Result<void> TriggerControllerAdapter::SetRangeTrigger(LogicalAxisId axis,
                                                       float32 start_pos,
                                                       float32 end_pos,
                                                       int32 pulse_width_us) {
    auto port_ready = EnsurePortReady(hardware_test_port_);
    if (port_ready.IsError()) {
        return port_ready;
    }

    if (end_pos <= start_pos) {
        return Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_PARAMETER,
                                 "Invalid trigger range",
                                 "TriggerControllerAdapter"));
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.mode = TriggerMode::RANGE;
        config_.pulse_width_us = pulse_width_us;
    }

    auto clear_result = ClearTrigger(axis);
    if (clear_result.IsError()) {
        return clear_result;
    }

    auto start_result = AddTriggerPoint(axis, start_pos, ResolveActionForMode(TriggerMode::RANGE, false));
    if (start_result.IsError()) {
        return start_result;
    }

    auto end_result = AddTriggerPoint(axis, end_pos, ResolveActionForMode(TriggerMode::RANGE, true));
    if (end_result.IsError()) {
        return end_result;
    }

    return EnableTrigger(axis);
}

Result<void> TriggerControllerAdapter::SetSequenceTrigger(LogicalAxisId axis,
                                                          const std::vector<float32>& positions,
                                                          int32 pulse_width_us) {
    auto port_ready = EnsurePortReady(hardware_test_port_);
    if (port_ready.IsError()) {
        return port_ready;
    }

    if (positions.empty()) {
        return Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_PARAMETER,
                                 "Trigger positions list is empty",
                                 "TriggerControllerAdapter"));
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        config_.mode = TriggerMode::CONTINUOUS;
        config_.pulse_width_us = pulse_width_us;
    }

    auto clear_result = ClearTrigger(axis);
    if (clear_result.IsError()) {
        return clear_result;
    }

    std::vector<float32> sorted_positions = positions;
    std::sort(sorted_positions.begin(), sorted_positions.end());
    sorted_positions.erase(std::unique(sorted_positions.begin(), sorted_positions.end()), sorted_positions.end());

    for (const auto pos : sorted_positions) {
        auto add_result = AddTriggerPoint(axis, pos, TriggerAction::TurnOn);
        if (add_result.IsError()) {
            return add_result;
        }
    }

    return EnableTrigger(axis);
}

Result<void> TriggerControllerAdapter::EnableTrigger(LogicalAxisId axis) {
    auto port_ready = EnsurePortReady(hardware_test_port_);
    if (port_ready.IsError()) {
        return port_ready;
    }

    const short axis_index = static_cast<short>(Siligen::Shared::Types::ToIndex(axis));
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = trigger_states_.find(axis_index);
    if (it == trigger_states_.end() || it->second.trigger_ids.empty()) {
        return Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_STATE,
                                 "No trigger configured for axis",
                                 "TriggerControllerAdapter"));
    }

    for (const auto& id : it->second.trigger_ids) {
        auto result = hardware_test_port_->enableTrigger(id);
        if (result.IsError()) {
            return result;
        }
    }

    it->second.enabled = true;
    return Result<void>::Success();
}

Result<void> TriggerControllerAdapter::DisableTrigger(LogicalAxisId axis) {
    auto port_ready = EnsurePortReady(hardware_test_port_);
    if (port_ready.IsError()) {
        return port_ready;
    }

    const short axis_index = static_cast<short>(Siligen::Shared::Types::ToIndex(axis));
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = trigger_states_.find(axis_index);
    if (it == trigger_states_.end() || it->second.trigger_ids.empty()) {
        return Result<void>::Failure(
            Shared::Types::Error(Shared::Types::ErrorCode::INVALID_STATE,
                                 "No trigger configured for axis",
                                 "TriggerControllerAdapter"));
    }

    for (const auto& id : it->second.trigger_ids) {
        auto result = hardware_test_port_->disableTrigger(id);
        if (result.IsError()) {
            return result;
        }
    }

    it->second.enabled = false;
    return Result<void>::Success();
}

Result<void> TriggerControllerAdapter::ClearTrigger(LogicalAxisId axis) {
    auto port_ready = EnsurePortReady(hardware_test_port_);
    if (port_ready.IsError()) {
        return port_ready;
    }

    auto clear_result = hardware_test_port_->clearAllTriggers();
    if (clear_result.IsError()) {
        return clear_result;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    trigger_states_.clear();
    (void)axis;
    return Result<void>::Success();
}

Result<TriggerStatus> TriggerControllerAdapter::GetTriggerStatus(LogicalAxisId axis) const {
    auto port_ready = EnsurePortReady(hardware_test_port_);
    if (port_ready.IsError()) {
        return Result<TriggerStatus>::Failure(port_ready.GetError());
    }

    const short axis_index = static_cast<short>(Siligen::Shared::Types::ToIndex(axis));
    TriggerStatus status;
    std::vector<int> trigger_ids;
    std::vector<float32> positions;
    bool enabled = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = trigger_states_.find(axis_index);
        if (it != trigger_states_.end()) {
            trigger_ids = it->second.trigger_ids;
            positions = it->second.trigger_positions;
            enabled = it->second.enabled;
        }
    }

    status.is_enabled = enabled;

    if (!trigger_ids.empty()) {
        std::unordered_set<int> id_lookup(trigger_ids.begin(), trigger_ids.end());
        const auto events = hardware_test_port_->getTriggerEvents();
        for (const auto& event : events) {
            if (id_lookup.count(event.triggerPointId) > 0) {
                status.trigger_count++;
                status.last_trigger_position = static_cast<float32>(event.actualPosition);
            }
        }

        if (status.trigger_count == 0 && !positions.empty()) {
            status.last_trigger_position = positions.back();
        }
    }

    return Result<TriggerStatus>::Success(status);
}

Result<bool> TriggerControllerAdapter::IsTriggerEnabled(LogicalAxisId axis) const {
    const short axis_index = static_cast<short>(Siligen::Shared::Types::ToIndex(axis));
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = trigger_states_.find(axis_index);
    if (it == trigger_states_.end()) {
        return Result<bool>::Success(false);
    }
    return Result<bool>::Success(it->second.enabled);
}

Result<int32> TriggerControllerAdapter::GetTriggerCount(LogicalAxisId axis) const {
    auto status = GetTriggerStatus(axis);
    if (status.IsError()) {
        return Result<int32>::Failure(status.GetError());
    }
    return Result<int32>::Success(status.Value().trigger_count);
}

Result<void> TriggerControllerAdapter::AddTriggerPoint(LogicalAxisId axis, float32 position, TriggerAction action) {
    auto port_ready = EnsurePortReady(hardware_test_port_);
    if (port_ready.IsError()) {
        return port_ready;
    }

    int output_port = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        output_port = config_.channel;
    }

    auto configure_result =
        hardware_test_port_->configureTriggerPoint(axis,
                                                   static_cast<double>(position),
                                                   output_port,
                                                   action);
    if (configure_result.IsError()) {
        return Result<void>::Failure(configure_result.GetError());
    }

    const short axis_index = static_cast<short>(Siligen::Shared::Types::ToIndex(axis));
    std::lock_guard<std::mutex> lock(mutex_);
    auto& state = trigger_states_[axis_index];
    state.trigger_ids.push_back(configure_result.Value());
    state.trigger_positions.push_back(position);
    state.enabled = false;
    return Result<void>::Success();
}

}  // namespace Siligen::Infrastructure::Adapters::Dispensing

