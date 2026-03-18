#include "MotionMonitoringUseCase.h"

#include <utility>

using namespace Siligen::Shared::Types;

namespace Siligen::Application::UseCases::Motion::Monitoring {

namespace {
void ApplyHomingStatusOverride(
    const std::shared_ptr<Domain::Motion::Ports::IHomingPort>& homing_port,
    LogicalAxisId axis_id,
    Domain::Motion::Ports::MotionStatus& status
) {
    if (!homing_port) {
        return;
    }

    auto homing_result = homing_port->GetHomingStatus(axis_id);
    if (homing_result.IsError()) {
        return;
    }

    using Domain::Motion::Ports::HomingState;
    using Domain::Motion::Ports::MotionState;

    if (status.state == MotionState::ESTOP) {
        return;
    }

    switch (homing_result.Value().state) {
        case HomingState::HOMED:
            if (status.state != MotionState::FAULT) {
                status.state = MotionState::HOMED;
            }
            break;
        case HomingState::HOMING:
            if (status.state != MotionState::FAULT) {
                status.state = MotionState::HOMING;
            }
            break;
        case HomingState::FAILED:
            status.state = MotionState::FAULT;
            status.has_error = true;
            break;
        case HomingState::NOT_HOMED:
            if (status.state == MotionState::HOMED) {
                status.state = MotionState::IDLE;
            }
            break;
        default:
            break;
    }
}
}  // namespace

MotionMonitoringUseCase::MotionMonitoringUseCase(
    std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port,
    std::shared_ptr<Domain::Motion::Ports::IIOControlPort> io_port,
    std::shared_ptr<Domain::Motion::Ports::IHomingPort> homing_port)
    : motion_state_port_(std::move(motion_state_port))
    , homing_port_(std::move(homing_port))
    , io_port_(io_port)
    , status_update_running_(false)
    , status_update_interval_ms_(100) {
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
    ApplyHomingStatusOverride(homing_port_, axis_id, status);
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
        ApplyHomingStatusOverride(homing_port_, axis_id, statuses[i]);
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

Result<Domain::Motion::Ports::IOStatus> MotionMonitoringUseCase::ReadDigitalInputStatus(int16 channel) const {
    auto validation = ValidateChannelNumber(channel);
    if (validation.IsError()) {
        return Result<Domain::Motion::Ports::IOStatus>::Failure(validation.GetError());
    }

    if (!io_port_) {
        return Result<Domain::Motion::Ports::IOStatus>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "IO port not initialized",
            "MotionMonitoringUseCase::ReadDigitalInputStatus"
        ));
    }

    return io_port_->ReadDigitalInput(channel);
}

Result<std::vector<Domain::Motion::Ports::IOStatus>> MotionMonitoringUseCase::ReadAllDigitalInputStatus() const {
    if (!io_port_) {
        return Result<std::vector<Domain::Motion::Ports::IOStatus>>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "IO port not initialized",
            "MotionMonitoringUseCase::ReadAllDigitalInputStatus"
        ));
    }

    std::vector<Domain::Motion::Ports::IOStatus> statuses;
    for (int16 channel = 0; channel < 16; ++channel) {
        auto result = io_port_->ReadDigitalInput(channel);
        if (result.IsError()) {
            return Result<std::vector<Domain::Motion::Ports::IOStatus>>::Failure(result.GetError());
        }
        statuses.push_back(result.Value());
    }
    return Result<std::vector<Domain::Motion::Ports::IOStatus>>::Success(statuses);
}

Result<Domain::Motion::Ports::IOStatus> MotionMonitoringUseCase::ReadDigitalOutputStatus(int16 channel) const {
    auto validation = ValidateChannelNumber(channel);
    if (validation.IsError()) {
        return Result<Domain::Motion::Ports::IOStatus>::Failure(validation.GetError());
    }

    if (!io_port_) {
        return Result<Domain::Motion::Ports::IOStatus>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "IO port not initialized",
            "MotionMonitoringUseCase::ReadDigitalOutputStatus"
        ));
    }

    return io_port_->ReadDigitalOutput(channel);
}

Result<std::vector<Domain::Motion::Ports::IOStatus>> MotionMonitoringUseCase::ReadAllDigitalOutputStatus() const {
    if (!io_port_) {
        return Result<std::vector<Domain::Motion::Ports::IOStatus>>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "IO port not initialized",
            "MotionMonitoringUseCase::ReadAllDigitalOutputStatus"
        ));
    }

    std::vector<Domain::Motion::Ports::IOStatus> statuses;
    for (int16 channel = 0; channel < 16; ++channel) {
        auto result = io_port_->ReadDigitalOutput(channel);
        if (result.IsError()) {
            if (result.GetError().GetCode() == ErrorCode::NOT_IMPLEMENTED) {
                break;
            }
            return Result<std::vector<Domain::Motion::Ports::IOStatus>>::Failure(result.GetError());
        }
        statuses.push_back(result.Value());
    }
    return Result<std::vector<Domain::Motion::Ports::IOStatus>>::Success(statuses);
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
    motion_status_callback_ = std::move(callback);
}

void MotionMonitoringUseCase::SetIOStatusCallback(IOStatusCallback callback) {
    io_status_callback_ = std::move(callback);
}

Result<void> MotionMonitoringUseCase::StartStatusUpdate(int32 interval_ms) {
    status_update_interval_ms_ = interval_ms;
    status_update_running_ = true;
    return Result<void>::Success();
}

void MotionMonitoringUseCase::StopStatusUpdate() {
    status_update_running_ = false;
}

bool MotionMonitoringUseCase::IsStatusUpdateRunning() const {
    return status_update_running_;
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
    if (motion_status_callback_) {
        motion_status_callback_(axis_id, status);
    }
}

void MotionMonitoringUseCase::NotifyIOStatusUpdate(const Domain::Motion::Ports::IOStatus& signal) {
    if (io_status_callback_) {
        io_status_callback_(signal);
    }
}

void MotionMonitoringUseCase::StatusUpdateTimer() {
    if (!status_update_running_) {
        return;
    }

    auto allStatusResult = GetAllAxesMotionStatus();
    if (allStatusResult.IsSuccess()) {
        const auto& allStatus = allStatusResult.Value();
        for (size_t i = 0; i < allStatus.size(); ++i) {
            auto axis_id = FromIndex(static_cast<int16>(i));
            if (IsValid(axis_id)) {
                NotifyMotionStatusUpdate(axis_id, allStatus[i]);
            }
        }
    }

    auto allIOResult = ReadAllDigitalInputStatus();
    if (allIOResult.IsSuccess()) {
        const auto& allIO = allIOResult.Value();
        for (const auto& io : allIO) {
            NotifyIOStatusUpdate(io);
        }
    }
}

}  // namespace Siligen::Application::UseCases::Motion::Monitoring



