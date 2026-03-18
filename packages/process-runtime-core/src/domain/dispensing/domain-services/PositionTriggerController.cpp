#include "PositionTriggerController.h"

#include "../../motion/CMPValidator.h"

namespace Siligen {

PositionTriggerController::PositionTriggerController(std::shared_ptr<MultiCardInterface> hw)
    : hardware_interface_(hw), multi_card_direct_(nullptr), is_initialized_(false), buffer_enabled_(false) {
    InitializeChannels();
}

PositionTriggerController::PositionTriggerController(MultiCard* mc)
    : hardware_interface_(nullptr), multi_card_direct_(mc), is_initialized_(false), buffer_enabled_(false) {
    InitializeChannels();
}

PositionTriggerController::~PositionTriggerController() {
    if (is_initialized_) {
        Shutdown();
    }
}

void PositionTriggerController::InitializeChannels() {
    cmp_configurations_.resize(2);
    trigger_points_map_.resize(2);
    channel_status_map_.resize(2);

    for (int i = 0; i < 2; i++) {
        cmp_configurations_[i].channel = static_cast<int16>(i + 1);
        cmp_configurations_[i].ResetToDefaults();
        channel_status_map_[i].channel = static_cast<int16>(i + 1);
    }
}

bool PositionTriggerController::Initialize() {
    if (is_initialized_) {
        return true;
    }

    if (!hardware_interface_) {
        NotifyError(0, "Hardware interface not available");
        return false;
    }

    for (const auto& config : cmp_configurations_) {
        if (!ConfigureCMPOutput(config)) {
            return false;
        }
    }

    is_initialized_ = true;
    return true;
}

bool PositionTriggerController::Shutdown() {
    if (!is_initialized_) {
        return true;
    }

    StopTrigger(0);
    ClearAllTriggerPoints();
    is_initialized_ = false;
    return true;
}

bool PositionTriggerController::ConfigureCMPOutput(const CMPPulseConfiguration& config) {
    if (config.channel < 1 || config.channel > 2 || !config.IsValid()) {
        return false;
    }

    // TODO: Implement hardware call when MultiCardInterface is properly defined
    // short mask = 1 << (config.channel - 1);
    // int result = hardware_interface_->MC_CmpPluse(...);

    cmp_configurations_[static_cast<size_t>(config.channel - 1)] = config;
    channel_status_map_[static_cast<size_t>(config.channel - 1)].is_configured = true;
    return true;
}

CMPPulseConfiguration PositionTriggerController::GetCMPConfiguration(short channel) const {
    if (channel < 1 || channel > 2) {
        return CMPPulseConfiguration();
    }
    return cmp_configurations_[static_cast<size_t>(channel - 1)];
}

bool PositionTriggerController::SetSinglePointTrigger(int32 position,
                                                      DispensingAction action,
                                                      int32 pulse_width_us,
                                                      int32 delay_time_us) {
    CMPTriggerPoint point(position, action, pulse_width_us, delay_time_us);
    if (!point.Validate()) {
        return false;
    }

    trigger_points_map_[0] = {point};
    return true;
}

bool PositionTriggerController::SetRangeTrigger(int32 start, int32 end, int32 pulse_width_us) {
    if (start >= end) {
        return false;
    }

    trigger_points_map_[0] = {CMPTriggerPoint(start, DispensingAction::CONTINUOUS, pulse_width_us),
                              CMPTriggerPoint(end, DispensingAction::NONE, pulse_width_us)};
    return true;
}

bool PositionTriggerController::SetSequenceTrigger(const std::vector<CMPTriggerPoint>& points) {
    if (points.empty()) {
        return false;
    }

    for (const auto& p : points) {
        if (!p.Validate()) {
            return false;
        }
    }

    trigger_points_map_[0] = points;
    return true;
}

bool PositionTriggerController::SetRepeatTrigger(int32 position, int32 pulse_width_us, int32 interval_us, int32 count) {
    trigger_points_map_[0].clear();
    trigger_points_map_[0].push_back(CMPTriggerPoint(position, DispensingAction::PULSE, pulse_width_us));
    return true;
}

bool PositionTriggerController::ClearAllTriggerPoints() {
    for (auto& points : trigger_points_map_) {
        points.clear();
    }
    return true;
}

bool PositionTriggerController::ClearChannelTriggerPoints(short channel) {
    if (channel < 1 || channel > 2) {
        return false;
    }

    trigger_points_map_[static_cast<size_t>(channel - 1)].clear();
    return true;
}

bool PositionTriggerController::StartTrigger(short channel) {
    if (channel == 0) {
        return StartTrigger(1) && StartTrigger(2);
    }

    if (channel < 1 || channel > 2) {
        return false;
    }

    channel_status_map_[static_cast<size_t>(channel - 1)].is_active = true;
    return true;
}

bool PositionTriggerController::StopTrigger(short channel) {
    if (channel == 0) {
        return StopTrigger(1) && StopTrigger(2);
    }

    if (channel < 1 || channel > 2) {
        return false;
    }

    // TODO: Implement hardware call when MultiCardInterface is properly defined
    // int result = hardware_interface_->MC_CmpPluse(...);

    channel_status_map_[static_cast<size_t>(channel - 1)].is_active = false;
    return true;
}

bool PositionTriggerController::IsTriggerActive(short channel) const {
    if (channel < 1 || channel > 2) {
        return false;
    }

    return channel_status_map_[static_cast<size_t>(channel - 1)].is_active;
}

CMPOutputStatus PositionTriggerController::GetChannelStatus(short channel) const {
    if (channel < 1 || channel > 2) {
        return CMPOutputStatus();
    }

    return channel_status_map_[static_cast<size_t>(channel - 1)];
}

std::vector<CMPTriggerPoint> PositionTriggerController::GetTriggerPoints(short channel) const {
    if (channel < 1 || channel > 2) {
        return std::vector<CMPTriggerPoint>{};
    }

    return trigger_points_map_[static_cast<size_t>(channel - 1)];
}

int32 PositionTriggerController::GetCurrentPosition() const {
    if (!hardware_interface_) {
        return 0;
    }

    return 0;
}

bool PositionTriggerController::EnableTriggerBuffering(short channel, bool enable) {
    if (channel < 1 || channel > 2) {
        return false;
    }

    buffer_enabled_ = enable;
    return true;
}

bool PositionTriggerController::SetTriggerCallback(std::function<void(short, int32, DispensingAction)> callback) {
    trigger_callback_ = callback;
    return true;
}

void PositionTriggerController::NotifyError(short channel, const std::string& message) {
    if (channel >= 1 && channel <= 2) {
        channel_status_map_[static_cast<size_t>(channel - 1)].last_error = message;
    }
}

bool PositionTriggerController::ProcessTriggerPoint(short channel, int32 current_position) {
    if (channel < 1 || channel > 2) {
        return false;
    }

    const auto& points = trigger_points_map_[static_cast<size_t>(channel - 1)];
    for (const auto& point : points) {
        if (point.position == current_position && point.is_enabled) {
            if (trigger_callback_) {
                trigger_callback_(channel, point.position, point.action);
            }
            channel_status_map_[static_cast<size_t>(channel - 1)].trigger_count++;
            return true;
        }
    }

    return false;
}

}  // namespace Siligen
