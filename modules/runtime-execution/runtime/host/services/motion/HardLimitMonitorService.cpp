// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#define MODULE_NAME "HardLimitMonitorService"

#include "HardLimitMonitorService.h"

#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Error.h"
#include "domain/safety/domain-services/InterlockPolicy.h"

#include <chrono>
#include <stdexcept>
#include <utility>

namespace Siligen::Application::Services::Motion {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::IsValid;
using Siligen::Shared::Types::ToUserDisplay;
using Siligen::Shared::Types::int16;

HardLimitMonitorService::HardLimitMonitorService(
    std::shared_ptr<Domain::Motion::Ports::IIOControlPort> io_port,
    std::shared_ptr<Domain::Motion::Ports::IPositionControlPort> position_control_port,
    const HardLimitMonitorConfig& config)
    : io_port_(std::move(io_port))
    , position_control_port_(std::move(position_control_port))
    , config_(config) {
    if (!io_port_ || !position_control_port_) {
        throw std::invalid_argument("IO control port and position control port cannot be null");
    }
}

HardLimitMonitorService::~HardLimitMonitorService() {
    Stop();
}

Result<void> HardLimitMonitorService::Start() noexcept {
    if (is_running_.load()) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_STATE,
            "Hard limit monitor is already running"));
    }

    if (!config_.enabled) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_STATE,
            "Hard limit monitor is disabled in configuration"));
    }

    stop_requested_.store(false);
    is_running_.store(true);
    last_triggered_.assign(config_.monitored_axes.size(), false);

    try {
        monitoring_thread_ = std::thread([this]() { MonitoringLoop(); });
    } catch (const std::exception& e) {
        is_running_.store(false);
        return Result<void>::Failure(Error(
            ErrorCode::UNKNOWN_ERROR,
            std::string("Failed to create monitoring thread: ") + e.what()));
    }

    return Result<void>::Success();
}

Result<void> HardLimitMonitorService::Stop() noexcept {
    if (!is_running_.load()) {
        return Result<void>::Success();
    }

    stop_requested_.store(true);
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }

    is_running_.store(false);
    return Result<void>::Success();
}

void HardLimitMonitorService::MonitoringLoop() noexcept {
    while (!stop_requested_.load()) {
        for (size_t i = 0; i < config_.monitored_axes.size(); ++i) {
            auto axis_id = config_.monitored_axes[i];
            if (!IsValid(axis_id)) {
                SILIGEN_LOG_WARNING("Hard limit monitor: invalid axis id " +
                                    std::to_string(static_cast<int16>(axis_id)));
                continue;
            }
            const auto axis_display = ToUserDisplay(axis_id);
            bool pos_triggered = false;
            bool neg_triggered = false;
            bool pos_valid = false;
            bool neg_valid = false;

            auto pos_result = io_port_->ReadLimitStatus(axis_id, true);
            if (pos_result.IsError()) {
                SILIGEN_LOG_WARNING("Hard limit monitor: ReadLimitStatus(+) failed for axis " +
                                    std::to_string(axis_display) + ": " + pos_result.GetError().GetMessage());
            } else {
                pos_triggered = pos_result.Value();
                pos_valid = true;
            }

            auto neg_result = io_port_->ReadLimitStatus(axis_id, false);
            if (neg_result.IsError()) {
                SILIGEN_LOG_WARNING("Hard limit monitor: ReadLimitStatus(-) failed for axis " +
                                    std::to_string(axis_display) + ": " + neg_result.GetError().GetMessage());
            } else {
                neg_triggered = neg_result.Value();
                neg_valid = true;
            }

            if (!pos_valid && !neg_valid) {
                continue;
            }

            bool triggered = Siligen::Domain::Safety::DomainServices::InterlockPolicy::IsHardLimitTriggered(
                pos_triggered, neg_triggered);
            if (triggered && !last_triggered_[i] && config_.emergency_stop_on_trigger) {
                SILIGEN_LOG_ERROR("Hard limit triggered on axis " + std::to_string(axis_display) +
                                  " (pos=" + std::to_string(pos_triggered) +
                                  ", neg=" + std::to_string(neg_triggered) + "), stopping axis");
                auto stop_result = position_control_port_->StopAxis(axis_id, true);
                if (stop_result.IsError()) {
                    SILIGEN_LOG_ERROR("Stop axis failed: " + stop_result.GetError().GetMessage());
                }
            }
            last_triggered_[i] = triggered;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(config_.monitoring_interval_ms));
    }
}

}  // namespace Siligen::Application::Services::Motion
