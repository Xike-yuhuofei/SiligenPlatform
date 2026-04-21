// Phase 3: 六边形架构日志系统 - 定义模块名称供日志宏使用
#define MODULE_NAME "ValveAdapter"

#include "ValveAdapter.h"
#include "../../../../../../../dispense-packaging/domain/dispensing/domain-services/DispenseCompensationService.h"
#include "shared/interfaces/ILoggingService.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

namespace Siligen {
namespace Infrastructure {
namespace Adapters {

using namespace Domain::Dispensing::Ports;
using namespace Shared::Types;

namespace {
using DispenserCapability = Siligen::Device::Contracts::Capabilities::DispenserCapability;
using DispenserCommand = Siligen::Device::Contracts::Commands::DispenserCommand;
using DispenserCommandKind = Siligen::Device::Contracts::Commands::DispenserCommandKind;
using DeviceDispenserState = Siligen::Device::Contracts::State::DispenserState;
using SharedKernelError = Siligen::SharedKernel::Error;
using SharedKernelErrorCode = Siligen::SharedKernel::ErrorCode;

SharedKernelError ToKernelError(const Siligen::Shared::Types::Error& error) {
    return SharedKernelError(
        static_cast<SharedKernelErrorCode>(static_cast<int32>(error.GetCode())),
        error.GetMessage(),
        error.GetModule());
}

Siligen::SharedKernel::VoidResult ToKernelVoidResult(const Result<void>& result) {
    if (result.IsError()) {
        return Siligen::SharedKernel::VoidResult::Failure(ToKernelError(result.GetError()));
    }
    return Siligen::SharedKernel::VoidResult::Success();
}

DispenserCapability BuildDispenserCapability() {
    DispenserCapability capability;
    capability.supports_prime = false;
    capability.supports_pause = true;
    capability.supports_resume = true;
    capability.supports_continuous_mode = true;
    capability.supports_in_motion_pulse_shot = true;
    return capability;
}

constexpr auto kPathTriggerPollInterval = std::chrono::milliseconds(2);
constexpr uint32 kMaxCoordinateReadFailures = 20;

bool IsWithinTolerance(long current, long target, long tolerance) noexcept {
    const auto delta = static_cast<long long>(current) - static_cast<long long>(target);
    return std::llabs(delta) <= static_cast<long long>(std::max<long>(0L, tolerance));
}

double Square(double value) noexcept {
    return value * value;
}

bool SegmentPassesTarget(long start_x,
                         long start_y,
                         long end_x,
                         long end_y,
                         long target_x,
                         long target_y,
                         long tolerance) noexcept {
    const double segment_dx = static_cast<double>(end_x) - static_cast<double>(start_x);
    const double segment_dy = static_cast<double>(end_y) - static_cast<double>(start_y);
    const double length_sq = Square(segment_dx) + Square(segment_dy);
    if (length_sq <= 1e-12) {
        return false;
    }

    const double target_dx = static_cast<double>(target_x) - static_cast<double>(start_x);
    const double target_dy = static_cast<double>(target_y) - static_cast<double>(start_y);
    const double projection = (target_dx * segment_dx + target_dy * segment_dy) / length_sq;
    if (projection < 0.0 || projection > 1.0) {
        return false;
    }

    const double nearest_x = static_cast<double>(start_x) + projection * segment_dx;
    const double nearest_y = static_cast<double>(start_y) + projection * segment_dy;
    const double distance_sq =
        Square(nearest_x - static_cast<double>(target_x)) +
        Square(nearest_y - static_cast<double>(target_y));
    return distance_sq <= Square(static_cast<double>(std::max<long>(0L, tolerance)));
}

bool HasReachedPathTriggerEvent(bool has_previous_position,
                                long previous_x,
                                long previous_y,
                                long current_x,
                                long current_y,
                                const PathTriggerEvent& event,
                                long tolerance) noexcept {
    if (IsWithinTolerance(current_x, event.x_position_pulse, tolerance) &&
        IsWithinTolerance(current_y, event.y_position_pulse, tolerance)) {
        return true;
    }

    if (!has_previous_position) {
        return false;
    }

    return SegmentPassesTarget(previous_x,
                               previous_y,
                               current_x,
                               current_y,
                               event.x_position_pulse,
                               event.y_position_pulse,
                               tolerance);
}
}  // namespace

// ============================================================
// 点胶阀控制实现
// ============================================================

Result<DispenserValveState> ValveAdapter::StartDispenser(
    const DispenserValveParams& params) noexcept
{
    try {
        StopProfileCompareWorker();
        JoinTimedDispenserThreadIfFinished();
        JoinPathTriggeredDispenserThreadIfFinished();

        std::lock_guard<std::mutex> lock(dispenser_mutex_);

        if (!params.IsValid()) {
            const std::string error_msg = params.GetValidationError();
            dispenser_state_.status = DispenserValveStatus::Error;
            dispenser_state_.errorMessage = error_msg;
            return Result<DispenserValveState>::Failure(
                Shared::Types::Error(ErrorCode::INVALID_PARAMETER, error_msg));
        }

        RefreshTimedDispenserStateIfCompleted();

        if (dispenser_state_.status == DispenserValveStatus::Running ||
            dispenser_state_.status == DispenserValveStatus::Paused) {
            return Result<DispenserValveState>::Failure(
                Shared::Types::Error(ErrorCode::INVALID_STATE, "点胶阀已在运行中"));
        }

        ResetDispenserHardwareState("StartDispenser", false);
        ResetProfileCompareTrackingState();

        SILIGEN_LOG_INFO("StartDispenser: 开始启动点胶阀");
        SILIGEN_LOG_INFO("参数: count=" + std::to_string(params.count) +
                         ", intervalMs=" + std::to_string(params.intervalMs) +
                         ", durationMs=" + std::to_string(params.durationMs));

        const auto steady_start_time = std::chrono::steady_clock::now();
        const auto system_start_time = std::chrono::system_clock::now();
        const auto start_timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            system_start_time.time_since_epoch()).count();

        Domain::Dispensing::DomainServices::DispenseCompensationService compensation_service;
        const auto adjusted_params =
            compensation_service.ApplyTimedCompensation(params, compensation_profile_, false);

        dispenser_state_.status = DispenserValveStatus::Running;
        dispenser_state_.completedCount = 0;
        dispenser_state_.totalCount = adjusted_params.count;
        dispenser_state_.remainingCount = adjusted_params.count;
        dispenser_state_.progress = 0.0f;
        dispenser_state_.errorMessage.clear();
        dispenser_continuous_ = false;
        dispenser_run_mode_ = DispenserRunMode::Timed;
        timed_dispenser_stop_requested_ = false;
        timed_dispenser_pause_requested_ = false;
        timed_dispenser_active_ = true;

        path_trigger_pause_requested_ = false;
        path_trigger_stop_requested_ = false;
        path_trigger_active_ = false;
        path_trigger_events_.clear();
        path_trigger_next_index_ = 0;
        path_trigger_start_level_ = 0;
        path_trigger_pulse_width_ms_ = 0;

        dispenser_params_ = adjusted_params;
        dispenser_start_time_ = steady_start_time;
        dispenser_elapsed_before_pause_ = 0;
        dispenser_state_.startTime = static_cast<uint64>(start_timestamp);

        if (timed_dispenser_thread_.joinable()) {
            dispenser_state_.status = DispenserValveStatus::Error;
            dispenser_state_.errorMessage = "Timed 点胶任务线程仍在占用，无法重复启动";
            dispenser_run_mode_ = DispenserRunMode::None;
            timed_dispenser_active_ = false;
            return Result<DispenserValveState>::Failure(
                Shared::Types::Error(ErrorCode::INVALID_STATE, dispenser_state_.errorMessage));
        }

        try {
            timed_dispenser_thread_ = std::thread([this]() { TimedDispenserLoop(); });
        }
        catch (const std::exception& e) {
            dispenser_state_.status = DispenserValveStatus::Error;
            dispenser_state_.errorMessage = std::string("启动 timed 点胶线程失败: ") + e.what();
            dispenser_run_mode_ = DispenserRunMode::None;
            timed_dispenser_active_ = false;
            return Result<DispenserValveState>::Failure(
                Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, dispenser_state_.errorMessage));
        }

        SILIGEN_LOG_INFO("StartDispenser: 已进入异步执行, count=" + std::to_string(adjusted_params.count) +
                         ", intervalMs=" + std::to_string(adjusted_params.intervalMs) +
                         ", durationMs=" + std::to_string(adjusted_params.durationMs));

        return Result<DispenserValveState>::Success(dispenser_state_);
    }
    catch (const std::exception& e) {
        dispenser_state_.status = DispenserValveStatus::Error;
        dispenser_state_.errorMessage = std::string("Exception: ") + e.what();
        return Result<DispenserValveState>::Failure(
            Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, dispenser_state_.errorMessage));
    }
}

Result<DispenserValveState> ValveAdapter::OpenDispenser() noexcept {
    try {
        StopProfileCompareWorker();
        JoinTimedDispenserThreadIfFinished();
        JoinPathTriggeredDispenserThreadIfFinished();

        std::lock_guard<std::mutex> lock(dispenser_mutex_);

        RefreshTimedDispenserStateIfCompleted();

        if (dispenser_state_.status == DispenserValveStatus::Running ||
            dispenser_state_.status == DispenserValveStatus::Paused) {
            return Result<DispenserValveState>::Failure(
                Shared::Types::Error(ErrorCode::INVALID_STATE, "点胶阀已在运行中"));
        }

        ResetDispenserHardwareState("OpenDispenser", false);
        ResetProfileCompareTrackingState();

        SILIGEN_LOG_INFO("OpenDispenser: 开始连续打开点胶阀");

        const int result = CallMC_SetCmpLevel(true);
        if (result != 0) {
            const std::string error_msg = CreateErrorMessage(result, "OpenDispenser");
            dispenser_state_.status = DispenserValveStatus::Error;
            dispenser_state_.errorMessage = error_msg;
            return Result<DispenserValveState>::Failure(
                Shared::Types::Error(ErrorCode::HARDWARE_ERROR, error_msg));
        }

        dispenser_state_.status = DispenserValveStatus::Running;
        dispenser_state_.completedCount = 0;
        dispenser_state_.totalCount = 0;
        dispenser_state_.remainingCount = 0;
        dispenser_state_.progress = 0.0f;
        dispenser_state_.errorMessage.clear();
        dispenser_continuous_ = true;
        dispenser_run_mode_ = DispenserRunMode::Continuous;

        dispenser_start_time_ = std::chrono::steady_clock::now();
        dispenser_elapsed_before_pause_ = 0;

        const auto now = std::chrono::system_clock::now();
        const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        dispenser_state_.startTime = static_cast<uint64>(timestamp);

        return Result<DispenserValveState>::Success(dispenser_state_);
    }
    catch (const std::exception& e) {
        dispenser_state_.status = DispenserValveStatus::Error;
        dispenser_state_.errorMessage = std::string("Exception: ") + e.what();
        return Result<DispenserValveState>::Failure(
            Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, dispenser_state_.errorMessage));
    }
}

Result<void> ValveAdapter::CloseDispenser() noexcept {
    try {
        StopProfileCompareWorker();
        std::lock_guard<std::mutex> lock(dispenser_mutex_);

        if (dispenser_run_mode_ == DispenserRunMode::Timed ||
            dispenser_run_mode_ == DispenserRunMode::PositionTriggered) {
            return Result<void>::Failure(
                Shared::Types::Error(ErrorCode::INVALID_STATE, "非连续开阀模式请使用 stop 停止"));
        }

        if (dispenser_state_.status == DispenserValveStatus::Idle ||
            dispenser_state_.status == DispenserValveStatus::Stopped) {
            dispenser_continuous_ = false;
            dispenser_run_mode_ = DispenserRunMode::None;
            ResetDispenserHardwareState("CloseDispenser", false);
            ResetProfileCompareTrackingState();
            return Result<void>::Success();
        }

        SILIGEN_LOG_INFO("CloseDispenser: 关闭点胶阀");

        const int result = CallMC_SetCmpLevel(false);
        if (result != 0) {
            const std::string error_msg = CreateErrorMessage(result, "CloseDispenser");
            dispenser_state_.status = DispenserValveStatus::Error;
            dispenser_state_.errorMessage = error_msg;
            return Result<void>::Failure(
                Shared::Types::Error(ErrorCode::HARDWARE_ERROR, error_msg));
        }

        dispenser_state_.status = DispenserValveStatus::Stopped;
        dispenser_state_.errorMessage.clear();
        dispenser_continuous_ = false;
        dispenser_run_mode_ = DispenserRunMode::None;
        ResetProfileCompareTrackingState();

        ResetDispenserHardwareState("CloseDispenser", false);

        return Result<void>::Success();
    }
    catch (const std::exception& e) {
        return Result<void>::Failure(
            Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, std::string("Exception: ") + e.what()));
    }
}

Result<DispenserValveState> ValveAdapter::StartPositionTriggeredDispenser(
    const PositionTriggeredDispenserParams& params) noexcept
{
    try {
        StopProfileCompareWorker();
        JoinTimedDispenserThreadIfFinished();
        JoinPathTriggeredDispenserThreadIfFinished();

        std::lock_guard<std::mutex> lock(dispenser_mutex_);

        if (!params.IsValid()) {
            const std::string error_msg = params.GetValidationError();
            dispenser_state_.status = DispenserValveStatus::Error;
            dispenser_state_.errorMessage = error_msg;
            return Result<DispenserValveState>::Failure(
                Shared::Types::Error(ErrorCode::INVALID_PARAMETER, error_msg));
        }

        RefreshTimedDispenserStateIfCompleted();

        if (dispenser_state_.status == DispenserValveStatus::Running ||
            dispenser_state_.status == DispenserValveStatus::Paused) {
            return Result<DispenserValveState>::Failure(
                Shared::Types::Error(ErrorCode::INVALID_STATE, "点胶阀已在运行中"));
        }

        ResetDispenserHardwareState("StartPositionTriggeredDispenser", false);
        ResetProfileCompareTrackingState();

        Domain::Dispensing::DomainServices::DispenseCompensationService compensation_service;
        const auto adjusted_params =
            compensation_service.ApplyPositionCompensation(params, compensation_profile_, false);

        if (compensation_profile_.retract_enabled) {
            const int retract_result = CallMC_SetCmpLevel(false);
            if (retract_result != 0) {
                SILIGEN_LOG_WARNING("StartPositionTriggeredDispenser: 回吸关阀指令失败, code=" +
                                    std::to_string(retract_result));
            }
        }

        dispenser_state_.status = DispenserValveStatus::Running;
        dispenser_state_.completedCount = 0;
        dispenser_state_.totalCount = static_cast<uint32>(adjusted_params.trigger_events.size());
        dispenser_state_.remainingCount = dispenser_state_.totalCount;
        dispenser_state_.progress = 0.0f;
        dispenser_state_.errorMessage.clear();
        dispenser_continuous_ = false;
        dispenser_run_mode_ = DispenserRunMode::PositionTriggered;
        timed_dispenser_active_ = false;
        timed_dispenser_pause_requested_ = false;
        timed_dispenser_stop_requested_ = false;

        path_trigger_coordinate_system_ = adjusted_params.coordinate_system;
        path_trigger_start_level_ = adjusted_params.start_level;
        path_trigger_pulse_width_ms_ = adjusted_params.pulse_width_ms;
        path_trigger_position_tolerance_pulse_ = std::max<long>(0L, adjusted_params.position_tolerance_pulse);
        path_trigger_events_ = adjusted_params.trigger_events;
        path_trigger_next_index_ = 0;
        path_trigger_stop_requested_ = false;
        path_trigger_pause_requested_ = false;
        path_trigger_active_ = true;

        dispenser_start_time_ = std::chrono::steady_clock::now();
        dispenser_elapsed_before_pause_ = 0;

        const auto now = std::chrono::system_clock::now();
        const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        dispenser_state_.startTime = static_cast<uint64>(timestamp);

        if (path_trigger_thread_.joinable()) {
            dispenser_state_.status = DispenserValveStatus::Error;
            dispenser_state_.errorMessage = "Path-trigger 点胶任务线程仍在占用，无法重复启动";
            dispenser_run_mode_ = DispenserRunMode::None;
            path_trigger_active_ = false;
            return Result<DispenserValveState>::Failure(
                Shared::Types::Error(ErrorCode::INVALID_STATE, dispenser_state_.errorMessage));
        }

        try {
            path_trigger_thread_ = std::thread([this]() { PathTriggeredDispenserLoop(); });
        }
        catch (const std::exception& e) {
            dispenser_state_.status = DispenserValveStatus::Error;
            dispenser_state_.errorMessage = std::string("启动 path-trigger 点胶线程失败: ") + e.what();
            dispenser_run_mode_ = DispenserRunMode::None;
            path_trigger_active_ = false;
            path_trigger_events_.clear();
            path_trigger_next_index_ = 0;
            path_trigger_start_level_ = 0;
            return Result<DispenserValveState>::Failure(
                Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, dispenser_state_.errorMessage));
        }

        SILIGEN_LOG_INFO("StartPositionTriggeredDispenser: events=" +
                         std::to_string(adjusted_params.trigger_events.size()) +
                         ", coordinate_system=" + std::to_string(adjusted_params.coordinate_system) +
                         ", start_level=" + std::to_string(adjusted_params.start_level) +
                         ", tolerance=" + std::to_string(path_trigger_position_tolerance_pulse_) +
                         ", pulse_width=" + std::to_string(adjusted_params.pulse_width_ms) + "ms");

        return Result<DispenserValveState>::Success(dispenser_state_);
    }
    catch (const std::exception& e) {
        dispenser_state_.status = DispenserValveStatus::Error;
        dispenser_state_.errorMessage = std::string("Exception: ") + e.what();
        return Result<DispenserValveState>::Failure(
            Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, dispenser_state_.errorMessage));
    }
}

Result<void> ValveAdapter::StopDispenser() noexcept {
    try {
        StopProfileCompareWorker();
        StopTimedDispenserThread();
        StopPathTriggeredDispenserThread();

        std::lock_guard<std::mutex> lock(dispenser_mutex_);

        if (dispenser_state_.status == DispenserValveStatus::Idle ||
            dispenser_state_.status == DispenserValveStatus::Stopped) {
            dispenser_continuous_ = false;
            dispenser_run_mode_ = DispenserRunMode::None;
            path_trigger_events_.clear();
            path_trigger_next_index_ = 0;
            path_trigger_start_level_ = 0;
            path_trigger_pulse_width_ms_ = 0;
            ResetDispenserHardwareState("StopDispenser", false);
            ResetProfileCompareTrackingState();
            return Result<void>::Success();
        }

        const int reset_result = ResetDispenserHardwareState("StopDispenser", false);
        if (reset_result != 0) {
            SILIGEN_LOG_WARNING("StopDispenser: ResetDispenserHardwareState 返回非零值 " +
                                std::to_string(reset_result) + ",继续执行");
        }

        UpdateDispenserProgress();
        dispenser_state_.status = DispenserValveStatus::Stopped;
        dispenser_state_.errorMessage.clear();
        dispenser_continuous_ = false;
        dispenser_run_mode_ = DispenserRunMode::None;

        timed_dispenser_active_ = false;
        timed_dispenser_pause_requested_ = false;
        timed_dispenser_stop_requested_ = false;

        path_trigger_active_ = false;
        path_trigger_pause_requested_ = false;
        path_trigger_stop_requested_ = false;
        path_trigger_events_.clear();
        path_trigger_next_index_ = 0;
        path_trigger_start_level_ = 0;
        path_trigger_pulse_width_ms_ = 0;
        ResetProfileCompareTrackingState();

        SILIGEN_LOG_INFO("StopDispenser: completedCount=" +
                         std::to_string(dispenser_state_.completedCount) +
                         ", totalCount=" + std::to_string(dispenser_state_.totalCount));

        return Result<void>::Success();
    }
    catch (const std::exception& e) {
        return Result<void>::Failure(
            Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, std::string("Exception: ") + e.what()));
    }
}

Result<void> ValveAdapter::PauseDispenser() noexcept {
    try {
        std::lock_guard<std::mutex> lock(dispenser_mutex_);

        if (dispenser_continuous_) {
            return Result<void>::Failure(
                Shared::Types::Error(ErrorCode::INVALID_STATE, "连续开阀模式不支持暂停"));
        }

        if (dispenser_state_.status != DispenserValveStatus::Running) {
            return Result<void>::Failure(
                Shared::Types::Error(ErrorCode::INVALID_STATE, "点胶阀未在运行"));
        }

        if (dispenser_run_mode_ == DispenserRunMode::Timed) {
            timed_dispenser_pause_requested_ = true;

            int result = CallMC_StopCmpOutputs(BuildCmpChannelMask());
            if (result != 0) {
                const std::string error_msg = CreateErrorMessage(result, "PauseDispenser");
                dispenser_state_.status = DispenserValveStatus::Error;
                dispenser_state_.errorMessage = error_msg;
                return Result<void>::Failure(
                    Shared::Types::Error(ErrorCode::HARDWARE_ERROR, error_msg));
            }

            result = CallMC_SetCmpLevel(false);
            if (result != 0) {
                const std::string error_msg = CreateErrorMessage(result, "PauseDispenser");
                dispenser_state_.status = DispenserValveStatus::Error;
                dispenser_state_.errorMessage = error_msg;
                return Result<void>::Failure(
                    Shared::Types::Error(ErrorCode::HARDWARE_ERROR, error_msg));
            }

            UpdateDispenserProgress();
            dispenser_state_.status = DispenserValveStatus::Paused;
            dispenser_pause_time_ = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                dispenser_pause_time_ - dispenser_start_time_).count();
            dispenser_elapsed_before_pause_ = static_cast<uint32>(elapsed);

            SILIGEN_LOG_INFO("PauseDispenser(Timed): progress=" +
                             std::to_string(dispenser_state_.progress) + "%");
            timed_dispenser_cv_.notify_all();
            return Result<void>::Success();
        }

        if (dispenser_run_mode_ == DispenserRunMode::PositionTriggered) {
            return Result<void>::Failure(Shared::Types::Error(
                ErrorCode::INVALID_STATE,
                "位置触发点胶不支持单独暂停，请暂停生产任务"));
        }

        return Result<void>::Failure(
            Shared::Types::Error(ErrorCode::INVALID_STATE, "当前模式不支持暂停"));
    }
    catch (const std::exception& e) {
        return Result<void>::Failure(
            Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, std::string("Exception: ") + e.what()));
    }
}

Result<void> ValveAdapter::ResumeDispenser() noexcept {
    try {
        std::lock_guard<std::mutex> lock(dispenser_mutex_);

        if (dispenser_continuous_) {
            return Result<void>::Failure(
                Shared::Types::Error(ErrorCode::INVALID_STATE, "连续开阀模式不支持恢复"));
        }

        if (dispenser_state_.status != DispenserValveStatus::Paused) {
            return Result<void>::Failure(
                Shared::Types::Error(ErrorCode::INVALID_STATE, "点胶阀未处于暂停状态"));
        }

        if (dispenser_run_mode_ == DispenserRunMode::Timed) {
            if (dispenser_state_.remainingCount == 0) {
                dispenser_state_.status = DispenserValveStatus::Idle;
                dispenser_run_mode_ = DispenserRunMode::None;
                timed_dispenser_pause_requested_ = false;
                return Result<void>::Success();
            }

            dispenser_state_.status = DispenserValveStatus::Running;
            dispenser_state_.errorMessage.clear();
            timed_dispenser_pause_requested_ = false;
            timed_dispenser_stop_requested_ = false;
            dispenser_start_time_ = std::chrono::steady_clock::now() -
                std::chrono::milliseconds(dispenser_elapsed_before_pause_);

            SILIGEN_LOG_INFO("ResumeDispenser(Timed): remainingCount=" +
                             std::to_string(dispenser_state_.remainingCount));
            timed_dispenser_cv_.notify_all();
            return Result<void>::Success();
        }

        if (dispenser_run_mode_ == DispenserRunMode::PositionTriggered) {
            return Result<void>::Failure(Shared::Types::Error(
                ErrorCode::INVALID_STATE,
                "位置触发点胶不支持单独恢复，请恢复生产任务"));
        }

        return Result<void>::Failure(
            Shared::Types::Error(ErrorCode::INVALID_STATE, "当前模式不支持恢复"));
    }
    catch (const std::exception& e) {
        return Result<void>::Failure(
            Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, std::string("Exception: ") + e.what()));
    }
}

Siligen::SharedKernel::VoidResult ValveAdapter::Execute(const DispenserCommand& command) {
    {
        std::lock_guard<std::mutex> lock(dispenser_mutex_);
        last_execution_id_ = command.execution_id;
    }

    switch (command.kind) {
        case DispenserCommandKind::kPrime:
            return Siligen::SharedKernel::VoidResult::Failure(SharedKernelError(
                SharedKernelErrorCode::NOT_IMPLEMENTED,
                "Prime is not implemented in production ValveAdapter",
                "ValveAdapter"));
        case DispenserCommandKind::kStart: {
            const auto result = OpenDispenser();
            if (result.IsError()) {
                return Siligen::SharedKernel::VoidResult::Failure(ToKernelError(result.GetError()));
            }
            return Siligen::SharedKernel::VoidResult::Success();
        }
        case DispenserCommandKind::kPause:
            return ToKernelVoidResult(PauseDispenser());
        case DispenserCommandKind::kResume:
            return ToKernelVoidResult(ResumeDispenser());
        case DispenserCommandKind::kStop:
            return ToKernelVoidResult(StopDispenser());
    }

    return Siligen::SharedKernel::VoidResult::Failure(SharedKernelError(
        SharedKernelErrorCode::INVALID_PARAMETER,
        "unsupported dispenser command kind",
        "ValveAdapter"));
}

Siligen::SharedKernel::Result<DeviceDispenserState> ValveAdapter::ReadState() const {
    try {
        auto* self = const_cast<ValveAdapter*>(this);
        self->JoinTimedDispenserThreadIfFinished();

        std::lock_guard<std::mutex> lock(dispenser_mutex_);
        if (dispenser_run_mode_ == DispenserRunMode::Timed) {
            self->RefreshTimedDispenserStateIfCompleted();
        }

        DeviceDispenserState state;
        state.primed = false;
        state.running = dispenser_state_.status == DispenserValveStatus::Running;
        state.paused = dispenser_state_.status == DispenserValveStatus::Paused;
        state.execution_id = last_execution_id_;
        return Siligen::SharedKernel::Result<DeviceDispenserState>::Success(state);
    }
    catch (const std::exception& e) {
        return Siligen::SharedKernel::Result<DeviceDispenserState>::Failure(SharedKernelError(
            SharedKernelErrorCode::UNKNOWN_ERROR,
            std::string("Exception: ") + e.what(),
            "ValveAdapter"));
    }
}

Siligen::SharedKernel::Result<DispenserCapability> ValveAdapter::DescribeCapability() const {
    return Siligen::SharedKernel::Result<DispenserCapability>::Success(BuildDispenserCapability());
}

void ValveAdapter::TimedDispenserLoop() noexcept {
    while (true) {
        uint32 interval_ms = 0;
        uint32 duration_ms = 0;
        int16 channel = 0;

        {
            std::unique_lock<std::mutex> lock(dispenser_mutex_);
            timed_dispenser_cv_.wait(lock, [this]() {
                return timed_dispenser_stop_requested_ || !timed_dispenser_pause_requested_;
            });

            if (timed_dispenser_stop_requested_) {
                timed_dispenser_active_ = false;
                timed_dispenser_pause_requested_ = false;
                timed_dispenser_stop_requested_ = false;
                return;
            }

            if (dispenser_run_mode_ != DispenserRunMode::Timed ||
                dispenser_state_.status != DispenserValveStatus::Running) {
                timed_dispenser_active_ = false;
                timed_dispenser_pause_requested_ = false;
                timed_dispenser_stop_requested_ = false;
                return;
            }

            UpdateDispenserProgress();
            if (dispenser_state_.status == DispenserValveStatus::Idle ||
                dispenser_state_.completedCount >= dispenser_state_.totalCount) {
                timed_dispenser_active_ = false;
                timed_dispenser_pause_requested_ = false;
                timed_dispenser_stop_requested_ = false;
                return;
            }

            interval_ms = dispenser_params_.intervalMs;
            duration_ms = dispenser_params_.durationMs;
            channel = static_cast<int16>(dispenser_config_.cmp_channel);
        }

        const int result = CallMC_CmpPulseOnce(channel, duration_ms);
        if (result != 0) {
            std::lock_guard<std::mutex> lock(dispenser_mutex_);
            const std::string error_msg = CreateErrorMessage(result, "TimedDispenserLoop");
            dispenser_state_.status = DispenserValveStatus::Error;
            dispenser_state_.errorMessage = error_msg;
            dispenser_run_mode_ = DispenserRunMode::None;
            timed_dispenser_active_ = false;
            timed_dispenser_pause_requested_ = false;
            timed_dispenser_stop_requested_ = false;
            SILIGEN_LOG_ERROR("TimedDispenserLoop: 脉冲触发失败: " + error_msg);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(dispenser_mutex_);
            if (dispenser_state_.completedCount < dispenser_state_.totalCount) {
                ++dispenser_state_.completedCount;
            }
            UpdateDispenserProgress();
            if (dispenser_state_.status == DispenserValveStatus::Idle) {
                timed_dispenser_active_ = false;
                timed_dispenser_pause_requested_ = false;
                timed_dispenser_stop_requested_ = false;
                SILIGEN_LOG_INFO("TimedDispenserLoop: 点胶任务完成");
                return;
            }
        }

        if (interval_ms > 0) {
            std::unique_lock<std::mutex> lock(dispenser_mutex_);
            timed_dispenser_cv_.wait_for(lock, std::chrono::milliseconds(interval_ms), [this]() {
                return timed_dispenser_stop_requested_ || timed_dispenser_pause_requested_;
            });
            if (timed_dispenser_stop_requested_) {
                timed_dispenser_active_ = false;
                timed_dispenser_pause_requested_ = false;
                timed_dispenser_stop_requested_ = false;
                return;
            }
        }
    }
}

void ValveAdapter::PathTriggeredDispenserLoop() noexcept {
    bool has_previous_position = false;
    long previous_x_position = 0;
    long previous_y_position = 0;
    uint32 read_failure_count = 0;

    while (true) {
        short coordinate_system = 0;
        long tolerance_pulse = 0;
        uint32 pulse_width_ms = 0;

        {
            std::unique_lock<std::mutex> lock(dispenser_mutex_);
            path_trigger_cv_.wait(lock, [this]() {
                return path_trigger_stop_requested_ || !path_trigger_pause_requested_;
            });

            if (path_trigger_stop_requested_) {
                path_trigger_active_ = false;
                path_trigger_pause_requested_ = false;
                path_trigger_stop_requested_ = false;
                return;
            }

            if (dispenser_run_mode_ != DispenserRunMode::PositionTriggered) {
                path_trigger_active_ = false;
                path_trigger_pause_requested_ = false;
                path_trigger_stop_requested_ = false;
                return;
            }

            if (dispenser_state_.status != DispenserValveStatus::Running) {
                if (dispenser_state_.status == DispenserValveStatus::Paused) {
                    continue;
                }
                path_trigger_active_ = false;
                path_trigger_pause_requested_ = false;
                path_trigger_stop_requested_ = false;
                return;
            }

            if (path_trigger_next_index_ >= path_trigger_events_.size()) {
                path_trigger_active_ = false;
                path_trigger_pause_requested_ = false;
                path_trigger_stop_requested_ = false;
                return;
            }

            coordinate_system = path_trigger_coordinate_system_;
            tolerance_pulse = path_trigger_position_tolerance_pulse_;
            pulse_width_ms = path_trigger_pulse_width_ms_;
        }

        long current_x_position = 0;
        long current_y_position = 0;
        if (!ReadCoordinateSystemPositionPulse(coordinate_system, current_x_position, current_y_position)) {
            ++read_failure_count;
            if (read_failure_count >= kMaxCoordinateReadFailures) {
                std::lock_guard<std::mutex> lock(dispenser_mutex_);
                dispenser_state_.status = DispenserValveStatus::Error;
                dispenser_state_.errorMessage = "连续读取坐标系位置失败，无法执行路径触发点胶";
                dispenser_run_mode_ = DispenserRunMode::None;
                path_trigger_active_ = false;
                path_trigger_pause_requested_ = false;
                path_trigger_stop_requested_ = false;
                return;
            }

            std::this_thread::sleep_for(kPathTriggerPollInterval);
            continue;
        }

        read_failure_count = 0;

        while (true) {
            PathTriggerEvent next_event;
            {
                std::lock_guard<std::mutex> lock(dispenser_mutex_);
                if (dispenser_run_mode_ != DispenserRunMode::PositionTriggered ||
                    dispenser_state_.status != DispenserValveStatus::Running ||
                    path_trigger_next_index_ >= path_trigger_events_.size()) {
                    break;
                }
                next_event = path_trigger_events_[path_trigger_next_index_];
            }

            if (!HasReachedPathTriggerEvent(has_previous_position,
                                            previous_x_position,
                                            previous_y_position,
                                            current_x_position,
                                            current_y_position,
                                            next_event,
                                            tolerance_pulse)) {
                break;
            }

            {
                std::lock_guard<std::mutex> lock(dispenser_mutex_);
                if (dispenser_run_mode_ != DispenserRunMode::PositionTriggered ||
                    dispenser_state_.status != DispenserValveStatus::Running ||
                    path_trigger_pause_requested_ ||
                    path_trigger_stop_requested_ ||
                    path_trigger_next_index_ >= path_trigger_events_.size() ||
                    !(path_trigger_events_[path_trigger_next_index_] == next_event)) {
                    break;
                }

                const int pulse_result = CallMC_CmpPulseOnce(0, pulse_width_ms, path_trigger_start_level_);
                if (pulse_result != 0) {
                    const std::string error_msg =
                        CreateErrorMessage(pulse_result, "PathTriggeredDispenserLoop");
                    dispenser_state_.status = DispenserValveStatus::Error;
                    dispenser_state_.errorMessage = error_msg;
                    dispenser_run_mode_ = DispenserRunMode::None;
                    path_trigger_active_ = false;
                    path_trigger_pause_requested_ = false;
                    path_trigger_stop_requested_ = false;
                    SILIGEN_LOG_ERROR("PathTriggeredDispenserLoop: 脉冲触发失败: " + error_msg);
                    return;
                }

                ++path_trigger_next_index_;
                if (dispenser_state_.completedCount < dispenser_state_.totalCount) {
                    ++dispenser_state_.completedCount;
                }
                UpdateDispenserProgress();

                if (path_trigger_next_index_ >= path_trigger_events_.size()) {
                    path_trigger_active_ = false;
                    path_trigger_pause_requested_ = false;
                    path_trigger_stop_requested_ = false;
                    SILIGEN_LOG_INFO("PathTriggeredDispenserLoop: 路径触发任务完成, completedCount=" +
                                     std::to_string(dispenser_state_.completedCount));
                    return;
                }
            }
        }

        previous_x_position = current_x_position;
        previous_y_position = current_y_position;
        has_previous_position = true;
        std::this_thread::sleep_for(kPathTriggerPollInterval);
    }
}

void ValveAdapter::StopTimedDispenserThread() noexcept {
    std::thread timed_thread_to_join;
    {
        std::lock_guard<std::mutex> lock(dispenser_mutex_);
        if (!timed_dispenser_thread_.joinable() && !timed_dispenser_active_) {
            timed_dispenser_stop_requested_ = false;
            timed_dispenser_pause_requested_ = false;
            return;
        }
        timed_dispenser_stop_requested_ = true;
        timed_dispenser_pause_requested_ = false;
        timed_dispenser_cv_.notify_all();
        if (timed_dispenser_thread_.joinable()) {
            timed_thread_to_join = std::move(timed_dispenser_thread_);
        }
    }

    if (timed_thread_to_join.joinable()) {
        timed_thread_to_join.join();
    }

    {
        std::lock_guard<std::mutex> lock(dispenser_mutex_);
        timed_dispenser_active_ = false;
        timed_dispenser_stop_requested_ = false;
        timed_dispenser_pause_requested_ = false;
    }
}

void ValveAdapter::JoinTimedDispenserThreadIfFinished() noexcept {
    std::thread finished_thread;
    {
        std::lock_guard<std::mutex> lock(dispenser_mutex_);
        if (timed_dispenser_active_ || !timed_dispenser_thread_.joinable()) {
            return;
        }
        finished_thread = std::move(timed_dispenser_thread_);
    }

    if (finished_thread.joinable()) {
        finished_thread.join();
    }
}

void ValveAdapter::StopPathTriggeredDispenserThread() noexcept {
    std::thread path_thread_to_join;
    {
        std::lock_guard<std::mutex> lock(dispenser_mutex_);
        if (!path_trigger_thread_.joinable() && !path_trigger_active_) {
            path_trigger_stop_requested_ = false;
            path_trigger_pause_requested_ = false;
            return;
        }
        path_trigger_stop_requested_ = true;
        path_trigger_pause_requested_ = false;
        path_trigger_cv_.notify_all();
        if (path_trigger_thread_.joinable()) {
            path_thread_to_join = std::move(path_trigger_thread_);
        }
    }

    if (path_thread_to_join.joinable()) {
        path_thread_to_join.join();
    }

    {
        std::lock_guard<std::mutex> lock(dispenser_mutex_);
        path_trigger_active_ = false;
        path_trigger_stop_requested_ = false;
        path_trigger_pause_requested_ = false;
    }
}

void ValveAdapter::JoinPathTriggeredDispenserThreadIfFinished() noexcept {
    std::thread finished_thread;
    {
        std::lock_guard<std::mutex> lock(dispenser_mutex_);
        if (path_trigger_active_ || !path_trigger_thread_.joinable()) {
            return;
        }
        finished_thread = std::move(path_trigger_thread_);
    }

    if (finished_thread.joinable()) {
        finished_thread.join();
    }
}

Result<DispenserValveState> ValveAdapter::GetDispenserStatus() noexcept {
    try {
        JoinTimedDispenserThreadIfFinished();
        JoinPathTriggeredDispenserThreadIfFinished();

        std::lock_guard<std::mutex> lock(dispenser_mutex_);
        if (dispenser_run_mode_ == DispenserRunMode::Timed ||
            dispenser_run_mode_ == DispenserRunMode::PositionTriggered) {
            UpdateDispenserProgress();
        }

        return Result<DispenserValveState>::Success(dispenser_state_);
    }
    catch (const std::exception& e) {
        return Result<DispenserValveState>::Failure(
            Shared::Types::Error(ErrorCode::UNKNOWN_ERROR, std::string("Exception: ") + e.what()));
    }
}

}  // namespace Adapters
}  // namespace Infrastructure
}  // namespace Siligen
