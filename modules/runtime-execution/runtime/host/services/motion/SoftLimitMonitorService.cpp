#define MODULE_NAME "SoftLimitMonitorService"

#include "SoftLimitMonitorService.h"

#include "domain/safety/domain-services/EmergencyStopService.h"
#include "domain/safety/domain-services/InterlockPolicy.h"
#include "runtime_execution/application/services/motion/MotionControlServiceImpl.h"
#include "runtime_execution/application/services/motion/MotionStatusServiceImpl.h"
#include "shared/errors/ErrorHandler.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Error.h"

#include <chrono>
#include <functional>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace Siligen::Application::Services::Motion {

using MotionControlServiceImpl = Siligen::RuntimeExecution::Application::Services::Motion::MotionControlServiceImpl;
using MotionStatusServiceImpl = Siligen::RuntimeExecution::Application::Services::Motion::MotionStatusServiceImpl;
using MachineExecutionStatePort = Siligen::RuntimeExecution::Contracts::System::IMachineExecutionStatePort;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::IsValid;
using Siligen::Shared::Types::ToUserDisplay;
using Siligen::Shared::Types::int16;
using Siligen::Shared::Types::int32;

namespace {

constexpr const char* kSoftLimitReasonPositive = "soft_limit_positive";
constexpr const char* kSoftLimitReasonNegative = "soft_limit_negative";

Error WrapSoftLimitMonitorFailure(
    const char* stage,
    const Error& error) {
    return Error(
        error.GetCode(),
        std::string("failure_stage=") + stage + ";failure_code=" +
            std::to_string(static_cast<int>(error.GetCode())) + ";message=" +
            error.GetMessage(),
        "SoftLimitMonitorService");
}

Result<void> BuildEmergencyStopFailure(const char* stage, const Error& error) {
    return Result<void>::Failure(Error(
        error.GetCode(),
        std::string("failure_stage=") + stage + ";failure_code=" +
            std::to_string(static_cast<int>(error.GetCode())) + ";message=" + error.GetMessage(),
        "SoftLimitMonitorService::StopMotion"));
}

Result<void> EnsureEmergencyStopStep(
    const char* stage,
    const Siligen::Domain::Safety::DomainServices::EmergencyStopStepResult& result) {
    if (result.success) {
        return Result<void>::Success();
    }
    return BuildEmergencyStopFailure(stage, result.error);
}

}  // namespace

SoftLimitMonitorService::SoftLimitMonitorService(
    std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port,
    std::shared_ptr<Domain::System::Ports::IEventPublisherPort> event_port,
    std::shared_ptr<Domain::Motion::Ports::IPositionControlPort> position_control_port,
    const SoftLimitMonitorConfig& config)
    : SoftLimitMonitorService(
          std::move(motion_state_port),
          std::move(event_port),
          std::move(position_control_port),
          nullptr,
          nullptr,
          config) {}

SoftLimitMonitorService::SoftLimitMonitorService(
    std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port,
    std::shared_ptr<Domain::System::Ports::IEventPublisherPort> event_port,
    std::shared_ptr<Domain::Motion::Ports::IPositionControlPort> position_control_port,
    std::shared_ptr<Domain::Dispensing::Ports::ITriggerControllerPort> trigger_port,
    std::shared_ptr<MachineExecutionStatePort> machine_execution_state_port,
    const SoftLimitMonitorConfig& config)
    : motion_state_port_(std::move(motion_state_port))
    , event_port_(std::move(event_port))
    , position_control_port_(std::move(position_control_port))
    , trigger_port_(std::move(trigger_port))
    , machine_execution_state_port_(std::move(machine_execution_state_port))
    , config_(config) {
    if (!motion_state_port_ || !event_port_ || !position_control_port_) {
        throw std::invalid_argument("Motion state port, event port, and position control port cannot be null");
    }
    if (config_.emergency_stop_on_trigger &&
        (!trigger_port_ || !machine_execution_state_port_)) {
        throw std::invalid_argument(
            "Trigger controller port and machine execution state port cannot be null when emergency stop path is enabled");
    }
}

SoftLimitMonitorService::~SoftLimitMonitorService() {
    Stop();
}

Result<void> SoftLimitMonitorService::Start() noexcept {
    if (is_running_.load()) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_STATE,
            "Soft limit monitor is already running"));
    }

    if (!config_.enabled) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_STATE,
            "Soft limit monitor is disabled in configuration"));
    }

    stop_requested_.store(false);
    is_running_.store(true);
    last_triggered_.assign(config_.monitored_axes.size(), SoftLimitSignalState{});

    try {
        monitoring_thread_ = std::thread([this]() {
            this->MonitoringLoop();
        });
    } catch (const std::exception& e) {
        is_running_.store(false);
        return Result<void>::Failure(Error(
            ErrorCode::UNKNOWN_ERROR,
            std::string("Failed to create monitoring thread: ") + e.what()));
    }

    return Result<void>::Success();
}

Result<void> SoftLimitMonitorService::Stop() noexcept {
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

Result<bool> SoftLimitMonitorService::CheckSoftLimits() noexcept {
    if (last_triggered_.size() != config_.monitored_axes.size()) {
        last_triggered_.assign(config_.monitored_axes.size(), SoftLimitSignalState{});
    }

    for (size_t index = 0; index < config_.monitored_axes.size(); ++index) {
        const auto axis_id = config_.monitored_axes[index];
        if (!IsValid(axis_id)) {
            continue;
        }
        auto status_result = motion_state_port_->GetAxisStatus(axis_id);
        if (!status_result.IsSuccess()) {
            continue;
        }

        const auto& status = status_result.Value();
        bool positive_limit = false;
        bool negative_limit = false;
        const bool triggered = Siligen::Domain::Safety::DomainServices::InterlockPolicy::IsSoftLimitTriggered(
            status, &positive_limit, &negative_limit);
        const SoftLimitSignalState current_state{positive_limit, negative_limit};
        const auto previous_state = index < last_triggered_.size()
            ? last_triggered_[index]
            : SoftLimitSignalState{};

        const bool positive_rising = positive_limit && !previous_state.positive;
        const bool negative_rising = negative_limit && !previous_state.negative;
        if (positive_rising || negative_rising) {
            auto handle_result = HandleSoftLimitTrigger(axis_id, status, positive_rising);
            if (handle_result.IsSuccess() ||
                handle_result.GetError().GetMessage().find("failure_stage=soft_limit_publish") != std::string::npos) {
                if (index < last_triggered_.size()) {
                    last_triggered_[index] = current_state;
                }
            }
            if (!handle_result.IsSuccess()) {
                return Result<bool>::Failure(handle_result.GetError());
            }
            return Result<bool>::Success(true);
        }
        if (index < last_triggered_.size()) {
            last_triggered_[index] = current_state;
        }
        if (!triggered && (previous_state.positive || previous_state.negative)) {
            SILIGEN_LOG_INFO(
                "Soft limit cleared on axis " + std::to_string(ToUserDisplay(axis_id)));
        }
    }

    return Result<bool>::Success(false);
}

void SoftLimitMonitorService::MonitoringLoop() noexcept {
    while (!stop_requested_.load()) {
        auto check_result = CheckSoftLimits();
        if (check_result.IsError()) {
            SILIGEN_LOG_ERROR("软限位处理失败: " + check_result.GetError().GetMessage());
        }
        std::this_thread::sleep_for(
            std::chrono::milliseconds(config_.monitoring_interval_ms));
    }
}

Result<void> SoftLimitMonitorService::HandleSoftLimitTrigger(
    LogicalAxisId axis_id,
    const Domain::Motion::Ports::MotionStatus& status,
    bool positive_limit) noexcept {
    std::optional<Error> first_failure;
    const int16 axis_display = ToUserDisplay(axis_id);

    Domain::System::Ports::SoftLimitTriggeredEvent event;
    event.type = Domain::System::Ports::EventType::SOFT_LIMIT_TRIGGERED;
    event.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    event.source = "SoftLimitMonitorService";
    {
        std::lock_guard<std::mutex> lock(task_mutex_);
        event.task_id = current_task_id_;
    }
    {
        std::lock_guard<std::mutex> lock(job_provider_mutex_);
        if (active_job_id_provider_) {
            event.job_id = active_job_id_provider_();
        }
    }
    event.axis = axis_id;
    event.position = status.axis_position_mm;
    event.is_positive_limit = positive_limit;
    event.reason_code = positive_limit ? kSoftLimitReasonPositive : kSoftLimitReasonNegative;
    event.error_code = positive_limit
        ? static_cast<int32>(Siligen::Shared::HardwareErrorCode::POSITIVE_SOFT_LIMIT)
        : static_cast<int32>(Siligen::Shared::HardwareErrorCode::NEGATIVE_SOFT_LIMIT);

    std::ostringstream oss;
    oss << "Soft limit "
        << (positive_limit ? "positive" : "negative")
        << " triggered on axis " << axis_display
        << " at axis position " << event.position << "mm"
        << ", reason_code=" << event.reason_code
        << ", error_code=" << event.error_code;
    if (!event.job_id.empty()) {
        oss << ", job_id=" << event.job_id;
    }
    event.message = oss.str();

    SILIGEN_LOG_ERROR(event.message);
    auto publish_result = event_port_->Publish(event);
    if (!publish_result.IsSuccess()) {
        SILIGEN_LOG_ERROR("软限位事件发布失败: " + publish_result.GetError().GetMessage());
        first_failure = WrapSoftLimitMonitorFailure("soft_limit_publish", publish_result.GetError());
    }

    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        if (trigger_callback_) {
            trigger_callback_(axis_id, event.position, positive_limit);
        }
    }

    if (config_.auto_stop_on_trigger) {
        auto stop_result = StopMotion(config_.emergency_stop_on_trigger);
        if (!stop_result.IsSuccess()) {
            SILIGEN_LOG_ERROR("软限位触发后的停机失败: " + stop_result.GetError().GetMessage());
            first_failure = WrapSoftLimitMonitorFailure("soft_limit_stop", stop_result.GetError());
        }
    }

    if (first_failure.has_value()) {
        return Result<void>::Failure(first_failure.value());
    }

    return Result<void>::Success();
}

Result<void> SoftLimitMonitorService::StopMotion(bool use_emergency_stop) noexcept {
    if (use_emergency_stop) {
        if (!position_control_port_ || !motion_state_port_) {
            return Result<void>::Failure(Error(
                ErrorCode::INVALID_STATE,
                "Motion stop path not fully initialized",
                "SoftLimitMonitorService::StopMotion"));
        }
        if (!trigger_port_ || !machine_execution_state_port_) {
            return Result<void>::Failure(Error(
                ErrorCode::PORT_NOT_INITIALIZED,
                "Emergency stop path missing trigger controller or machine execution state port",
                "SoftLimitMonitorService::StopMotion"));
        }

        auto motion_control_service = std::make_shared<MotionControlServiceImpl>(position_control_port_);
        auto motion_status_service = std::make_shared<MotionStatusServiceImpl>(motion_state_port_);
        Siligen::Domain::Safety::DomainServices::EmergencyStopService emergency_stop_service(
            std::move(motion_control_service),
            std::move(motion_status_service),
            trigger_port_,
            machine_execution_state_port_);

        Siligen::Domain::Safety::DomainServices::EmergencyStopOptions options;
        options.disable_cmp = true;
        options.clear_task_queue = true;
        options.disable_hardware = true;
        const auto outcome = emergency_stop_service.Execute(options);

        auto motion_stop_result = EnsureEmergencyStopStep("soft_limit_emergency_motion_stop", outcome.motion_stop);
        if (motion_stop_result.IsError()) {
            return motion_stop_result;
        }
        auto cmp_disable_result = EnsureEmergencyStopStep("soft_limit_disable_cmp", outcome.cmp_disable);
        if (cmp_disable_result.IsError()) {
            return cmp_disable_result;
        }
        auto task_clear_result = EnsureEmergencyStopStep("soft_limit_clear_pending_tasks", outcome.task_queue_clear);
        if (task_clear_result.IsError()) {
            return task_clear_result;
        }
        auto hardware_disable_result = EnsureEmergencyStopStep("soft_limit_disable_hardware", outcome.hardware_disable);
        if (hardware_disable_result.IsError()) {
            return hardware_disable_result;
        }
        auto state_update_result = EnsureEmergencyStopStep("soft_limit_state_update", outcome.state_update);
        if (state_update_result.IsError()) {
            return state_update_result;
        }
        return Result<void>::Success();
    }

    if (!position_control_port_) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_STATE,
            "Position control port not available",
            "SoftLimitMonitorService::StopMotion"));
    }

    return position_control_port_->StopAllAxes(false);
}

}  // namespace Siligen::Application::Services::Motion
