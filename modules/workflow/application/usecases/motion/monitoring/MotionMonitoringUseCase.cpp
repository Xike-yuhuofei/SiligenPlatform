#define MODULE_NAME "MotionMonitoringUseCase"

#include "MotionMonitoringUseCase.h"

#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"

#include <algorithm>
#include <utility>
#include <chrono>
#include <thread>

using namespace Siligen::Shared::Types;

namespace Siligen::Application::UseCases::Motion::Monitoring {

namespace {
const char* ToHomingStateLabel(Domain::Motion::Ports::HomingState state) {
    using Domain::Motion::Ports::HomingState;
    switch (state) {
        case HomingState::NOT_HOMED:
            return "not_homed";
        case HomingState::HOMING:
            return "homing";
        case HomingState::HOMED:
            return "homed";
        case HomingState::FAILED:
            return "failed";
        default:
            return "unknown";
    }
}

void AttachHomingState(
    const std::shared_ptr<Domain::Motion::Ports::IHomingPort>& homing_port,
    LogicalAxisId axis_id,
    Domain::Motion::Ports::MotionStatus& status
) {
    status.homing_state = "unknown";
    if (!homing_port) {
        return;
    }

    auto homing_result = homing_port->GetHomingStatus(axis_id);
    if (homing_result.IsError()) {
        return;
    }

    using Domain::Motion::Ports::HomingState;
    const auto homing_state = homing_result.Value().state;
    status.homing_state = ToHomingStateLabel(homing_state);
    if (homing_state == HomingState::FAILED) {
        status.has_error = true;
    }
}
}  // namespace

MotionMonitoringUseCase::MotionMonitoringUseCase(
    std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port,
    std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IIOControlPort> io_port,
    std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port,
    std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IInterpolationPort> interpolation_port)
    : motion_state_port_(std::move(motion_state_port))
    , homing_port_(std::move(homing_port))
    , io_port_(io_port)
    , interpolation_port_(std::move(interpolation_port)) {
}

MotionMonitoringUseCase::~MotionMonitoringUseCase() {
    StopStatusUpdate();
}

Result<Domain::Motion::Ports::MotionStatus> MotionMonitoringUseCase::GetAxisMotionStatus(LogicalAxisId axis_id) const {
    auto validation = ValidateAxisNumber(axis_id);
    if (validation.IsError()) {
        return Result<Domain::Motion::Ports::MotionStatus>::Failure(validation.GetError());
    }

    if (!motion_state_port_) {
        return Result<Domain::Motion::Ports::MotionStatus>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "Motion state port not initialized",
            "MotionMonitoringUseCase::GetAxisMotionStatus"
        ));
    }

    auto status_result = motion_state_port_->GetAxisStatus(axis_id);
    if (status_result.IsError()) {
        return Result<Domain::Motion::Ports::MotionStatus>::Failure(status_result.GetError());
    }
    auto status = status_result.Value();
    AttachHomingState(homing_port_, axis_id, status);
    return Result<Domain::Motion::Ports::MotionStatus>::Success(status);
}

Result<std::vector<Domain::Motion::Ports::MotionStatus>> MotionMonitoringUseCase::GetAllAxesMotionStatus() const {
    if (!motion_state_port_) {
        return Result<std::vector<Domain::Motion::Ports::MotionStatus>>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "Motion state port not initialized",
            "MotionMonitoringUseCase::GetAllAxesMotionStatus"
        ));
    }

    auto status_result = motion_state_port_->GetAllAxesStatus();
    if (status_result.IsError()) {
        return Result<std::vector<Domain::Motion::Ports::MotionStatus>>::Failure(status_result.GetError());
    }

    auto statuses = status_result.Value();
    for (size_t i = 0; i < statuses.size(); ++i) {
        auto axis_id = FromIndex(static_cast<int16>(i));
        if (!IsValid(axis_id)) {
            continue;
        }
        AttachHomingState(homing_port_, axis_id, statuses[i]);
    }
    return Result<std::vector<Domain::Motion::Ports::MotionStatus>>::Success(statuses);
}

Result<Point2D> MotionMonitoringUseCase::GetCurrentPosition() const {
    if (!motion_state_port_) {
        return Result<Point2D>::Failure(Error(
            ErrorCode::INVALID_STATE,
            "Motion state port not initialized",
            "MotionMonitoringUseCase::GetCurrentPosition"
        ));
    }

    return motion_state_port_->GetCurrentPosition();
}

Result<Siligen::RuntimeExecution::Contracts::Motion::CoordinateSystemStatus>
MotionMonitoringUseCase::GetCoordinateSystemStatus(int16 coord_sys) const {
    if (!interpolation_port_) {
        return Result<Siligen::RuntimeExecution::Contracts::Motion::CoordinateSystemStatus>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "Interpolation port not initialized",
            "MotionMonitoringUseCase::GetCoordinateSystemStatus"
        ));
    }
    return interpolation_port_->GetCoordinateSystemStatus(coord_sys);
}

Result<uint32> MotionMonitoringUseCase::GetInterpolationBufferSpace(int16 coord_sys) const {
    if (!interpolation_port_) {
        return Result<uint32>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "Interpolation port not initialized",
            "MotionMonitoringUseCase::GetInterpolationBufferSpace"
        ));
    }
    return interpolation_port_->GetInterpolationBufferSpace(coord_sys);
}

Result<uint32> MotionMonitoringUseCase::GetLookAheadBufferSpace(int16 coord_sys) const {
    if (!interpolation_port_) {
        return Result<uint32>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "Interpolation port not initialized",
            "MotionMonitoringUseCase::GetLookAheadBufferSpace"
        ));
    }
    return interpolation_port_->GetLookAheadBufferSpace(coord_sys);
}

Result<Siligen::RuntimeExecution::Contracts::Motion::IOStatus>
MotionMonitoringUseCase::ReadDigitalInputStatus(int16 channel) const {
    auto validation = ValidateChannelNumber(channel);
    if (validation.IsError()) {
        return Result<Siligen::RuntimeExecution::Contracts::Motion::IOStatus>::Failure(
            validation.GetError());
    }

    if (!io_port_) {
        return Result<Siligen::RuntimeExecution::Contracts::Motion::IOStatus>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "IO port not initialized",
            "MotionMonitoringUseCase::ReadDigitalInputStatus"
        ));
    }

    return io_port_->ReadDigitalInput(channel);
}

Result<std::vector<Siligen::RuntimeExecution::Contracts::Motion::IOStatus>>
MotionMonitoringUseCase::ReadAllDigitalInputStatus() const {
    if (!io_port_) {
        return Result<std::vector<Siligen::RuntimeExecution::Contracts::Motion::IOStatus>>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "IO port not initialized",
            "MotionMonitoringUseCase::ReadAllDigitalInputStatus"
        ));
    }

    std::vector<Siligen::RuntimeExecution::Contracts::Motion::IOStatus> statuses;
    for (int16 channel = 0; channel < 16; ++channel) {
        auto result = io_port_->ReadDigitalInput(channel);
        if (result.IsError()) {
            return Result<std::vector<Siligen::RuntimeExecution::Contracts::Motion::IOStatus>>::Failure(
                result.GetError());
        }
        statuses.push_back(result.Value());
    }
    return Result<std::vector<Siligen::RuntimeExecution::Contracts::Motion::IOStatus>>::Success(statuses);
}

Result<Siligen::RuntimeExecution::Contracts::Motion::IOStatus>
MotionMonitoringUseCase::ReadDigitalOutputStatus(int16 channel) const {
    auto validation = ValidateChannelNumber(channel);
    if (validation.IsError()) {
        return Result<Siligen::RuntimeExecution::Contracts::Motion::IOStatus>::Failure(
            validation.GetError());
    }

    if (!io_port_) {
        return Result<Siligen::RuntimeExecution::Contracts::Motion::IOStatus>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "IO port not initialized",
            "MotionMonitoringUseCase::ReadDigitalOutputStatus"
        ));
    }

    return io_port_->ReadDigitalOutput(channel);
}

Result<std::vector<Siligen::RuntimeExecution::Contracts::Motion::IOStatus>>
MotionMonitoringUseCase::ReadAllDigitalOutputStatus() const {
    if (!io_port_) {
        return Result<std::vector<Siligen::RuntimeExecution::Contracts::Motion::IOStatus>>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "IO port not initialized",
            "MotionMonitoringUseCase::ReadAllDigitalOutputStatus"
        ));
    }

    std::vector<Siligen::RuntimeExecution::Contracts::Motion::IOStatus> statuses;
    for (int16 channel = 0; channel < 16; ++channel) {
        auto result = io_port_->ReadDigitalOutput(channel);
        if (result.IsError()) {
            if (result.GetError().GetCode() == ErrorCode::NOT_IMPLEMENTED) {
                break;
            }
            return Result<std::vector<Siligen::RuntimeExecution::Contracts::Motion::IOStatus>>::Failure(
                result.GetError());
        }
        statuses.push_back(result.Value());
    }
    return Result<std::vector<Siligen::RuntimeExecution::Contracts::Motion::IOStatus>>::Success(statuses);
}

Result<bool> MotionMonitoringUseCase::ReadLimitStatus(LogicalAxisId axis_id, bool positive) const {
    auto validation = ValidateAxisNumber(axis_id);
    if (validation.IsError()) {
        return Result<bool>::Failure(validation.GetError());
    }

    if (!io_port_) {
        return Result<bool>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "IO port not initialized",
            "MotionMonitoringUseCase::ReadLimitStatus"
        ));
    }

    return io_port_->ReadLimitStatus(axis_id, positive);
}

Result<bool> MotionMonitoringUseCase::ReadServoAlarmStatus(LogicalAxisId axis_id) const {
    auto validation = ValidateAxisNumber(axis_id);
    if (validation.IsError()) {
        return Result<bool>::Failure(validation.GetError());
    }

    if (!io_port_) {
        return Result<bool>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "IO port not initialized",
            "MotionMonitoringUseCase::ReadServoAlarmStatus"
        ));
    }

    return io_port_->ReadServoAlarm(axis_id);
}

void MotionMonitoringUseCase::SetMotionStatusCallback(MotionStatusCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    motion_status_callback_ = std::move(callback);
}

void MotionMonitoringUseCase::SetIOStatusCallback(IOStatusCallback callback) {
    std::lock_guard<std::mutex> lock(callback_mutex_);
    io_status_callback_ = std::move(callback);
}

Result<void> MotionMonitoringUseCase::StartStatusUpdate(int32 interval_ms) {
    if (interval_ms <= 0) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "interval_ms must be greater than 0",
            "MotionMonitoringUseCase::StartStatusUpdate"
        ));
    }

    if (!motion_state_port_ && !io_port_) {
        return Result<void>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "Motion state port and IO port are both unavailable",
            "MotionMonitoringUseCase::StartStatusUpdate"
        ));
    }

    std::thread thread_to_join;
    {
        std::lock_guard<std::mutex> lock(status_update_lifecycle_mutex_);
        status_update_interval_ms_.store(interval_ms);
        if (status_update_thread_.joinable()) {
            if (status_update_thread_.get_id() == std::this_thread::get_id()) {
                // 避免在状态更新线程内自停自启造成 self-join 或双线程并发。
                stop_status_update_requested_.store(false);
                status_update_running_.store(true);
                return Result<void>::Success();
            }
            stop_status_update_requested_.store(true);
            thread_to_join = std::move(status_update_thread_);
        }
    }

    if (thread_to_join.joinable()) {
        thread_to_join.join();
    }

    {
        std::lock_guard<std::mutex> lock(status_update_lifecycle_mutex_);
        stop_status_update_requested_.store(false);
        status_update_running_.store(true);
        motion_status_failure_count_.store(0);
        io_status_failure_count_.store(0);
        motion_status_failure_logged_.store(false);
        io_status_failure_logged_.store(false);
        status_update_thread_ = std::thread([this]() { StatusUpdateLoop(); });
    }
    return Result<void>::Success();
}

void MotionMonitoringUseCase::StopStatusUpdate() {
    std::thread thread_to_join;
    bool called_from_worker_thread = false;
    {
        std::lock_guard<std::mutex> lock(status_update_lifecycle_mutex_);
        stop_status_update_requested_.store(true);
        if (status_update_thread_.joinable()) {
            if (status_update_thread_.get_id() == std::this_thread::get_id()) {
                // 由状态线程自身发起停止时仅设置停止标志，join 由外层线程负责。
                called_from_worker_thread = true;
            } else {
                thread_to_join = std::move(status_update_thread_);
            }
        } else {
            status_update_running_.store(false);
        }
    }
    if (thread_to_join.joinable()) {
        thread_to_join.join();
    }
    if (!called_from_worker_thread) {
        status_update_running_.store(false);
    }
}

bool MotionMonitoringUseCase::IsStatusUpdateRunning() const {
    return status_update_running_.load();
}

Result<void> MotionMonitoringUseCase::ValidateAxisNumber(LogicalAxisId axis_id) const {
    if (!IsValid(axis_id)) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "Invalid axis number",
            "MotionMonitoringUseCase::ValidateAxisNumber"
        ));
    }
    return Result<void>::Success();
}

Result<void> MotionMonitoringUseCase::ValidateChannelNumber(int16 channel) const {
    if (channel < 0 || channel > 127) {
        return Result<void>::Failure(Error(
            ErrorCode::INVALID_PARAMETER,
            "Channel number must be between 0 and 127",
            "MotionMonitoringUseCase::ValidateChannelNumber"
        ));
    }
    return Result<void>::Success();
}

void MotionMonitoringUseCase::NotifyMotionStatusUpdate(LogicalAxisId axis_id,
                                                       const Domain::Motion::Ports::MotionStatus& status) {
    MotionStatusCallback callback;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback = motion_status_callback_;
    }
    if (callback) {
        callback(axis_id, status);
    }
}

void MotionMonitoringUseCase::NotifyIOStatusUpdate(
    const Siligen::RuntimeExecution::Contracts::Motion::IOStatus& signal) {
    IOStatusCallback callback;
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        callback = io_status_callback_;
    }
    if (callback) {
        callback(signal);
    }
}

void MotionMonitoringUseCase::StatusUpdateLoop() {
    while (!stop_status_update_requested_.load()) {
        StatusUpdateTimer();

        const auto interval_ms = std::max(1, status_update_interval_ms_.load());
        const auto sleep_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(interval_ms);
        while (!stop_status_update_requested_.load() && std::chrono::steady_clock::now() < sleep_deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    status_update_running_.store(false);
}

void MotionMonitoringUseCase::StatusUpdateTimer() {
    if (motion_state_port_) {
        auto allStatusResult = GetAllAxesMotionStatus();
        if (allStatusResult.IsSuccess()) {
            const auto& allStatus = allStatusResult.Value();
            for (size_t i = 0; i < allStatus.size(); ++i) {
                auto axis_id = FromIndex(static_cast<int16>(i));
                if (IsValid(axis_id)) {
                    NotifyMotionStatusUpdate(axis_id, allStatus[i]);
                }
            }
            motion_status_failure_logged_.store(false);
        } else {
            const auto failure_count = motion_status_failure_count_.fetch_add(1) + 1;
            if (!motion_status_failure_logged_.exchange(true)) {
                SILIGEN_LOG_WARNING(
                    "failure_stage=motion_status_poll;failure_code=" +
                    std::to_string(static_cast<int>(allStatusResult.GetError().GetCode())) +
                    ";message=" + allStatusResult.GetError().GetMessage() +
                    ";failure_count=" + std::to_string(failure_count));
            }
        }
    }

    if (io_port_) {
        auto allIOResult = ReadAllDigitalInputStatus();
        if (allIOResult.IsSuccess()) {
            const auto& allIO = allIOResult.Value();
            for (const auto& io : allIO) {
                NotifyIOStatusUpdate(io);
            }
            io_status_failure_logged_.store(false);
        } else {
            const auto failure_count = io_status_failure_count_.fetch_add(1) + 1;
            if (!io_status_failure_logged_.exchange(true)) {
                SILIGEN_LOG_WARNING(
                    "failure_stage=io_status_poll;failure_code=" +
                    std::to_string(static_cast<int>(allIOResult.GetError().GetCode())) +
                    ";message=" + allIOResult.GetError().GetMessage() +
                    ";failure_count=" + std::to_string(failure_count));
            }
        }
    }
}

}  // namespace Siligen::Application::UseCases::Motion::Monitoring



