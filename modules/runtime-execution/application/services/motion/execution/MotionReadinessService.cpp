#include "runtime_execution/application/services/motion/execution/MotionReadinessService.h"

#include "shared/types/Error.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <thread>
#include <utility>

namespace Siligen::Application::Services::Motion::Execution {

namespace {

using MotionState = Siligen::Domain::Motion::Ports::MotionState;
using CoordinateSystemState = Siligen::Domain::Motion::Ports::CoordinateSystemState;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;

constexpr const char* kErrorSource = "MotionReadinessService";

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool IsBlockingJobState(const std::string& state) {
    const auto normalized = ToLower(state);
    return normalized == "running" ||
           normalized == "stopping" ||
           normalized == "canceling" ||
           normalized == "cancelling" ||
           normalized == "pausing" ||
           normalized == "paused";
}

bool HasVelocityAboveTolerance(const Siligen::Domain::Motion::Ports::MotionStatus& status, float32 tolerance) {
    return std::abs(status.velocity) > tolerance ||
           std::abs(status.profile_velocity_mm_s) > tolerance ||
           std::abs(status.encoder_velocity_mm_s) > tolerance;
}

bool HasBlockingAxisState(const Siligen::Domain::Motion::Ports::MotionStatus& status) {
    return status.state == MotionState::MOVING ||
           status.state == MotionState::HOMING ||
           status.state == MotionState::FAULT ||
           status.state == MotionState::ESTOP ||
           status.state == MotionState::DISABLED ||
           status.has_error ||
           status.servo_alarm ||
           status.following_error ||
           status.home_failed;
}

MotionReadinessResult MakeBlockedResult(
    const Siligen::Domain::Motion::Ports::CoordinateSystemStatus& coord_status,
    std::vector<Siligen::Domain::Motion::Ports::MotionStatus> axis_statuses,
    std::string diagnostic_message) {
    MotionReadinessResult result;
    result.ready = false;
    result.reason_code = "motion_not_ready";
    result.message = "motion_not_ready";
    result.diagnostic_message = std::move(diagnostic_message);
    result.coord_status = coord_status;
    result.axis_statuses = std::move(axis_statuses);
    return result;
}

}  // namespace

MotionReadinessService::MotionReadinessService(
    std::shared_ptr<Siligen::Domain::Motion::Ports::IMotionStatePort> motion_state_port,
    std::shared_ptr<Siligen::Domain::Motion::Ports::IInterpolationPort> interpolation_port)
    : motion_state_port_(std::move(motion_state_port)),
      interpolation_port_(std::move(interpolation_port)) {}

Result<MotionReadinessResult> MotionReadinessService::Evaluate(const MotionReadinessQuery& query) const {
    if (!motion_state_port_) {
        return Result<MotionReadinessResult>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Motion state port not initialized", kErrorSource));
    }
    if (!interpolation_port_) {
        return Result<MotionReadinessResult>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Interpolation port not initialized", kErrorSource));
    }

    auto coord_result = interpolation_port_->GetCoordinateSystemStatus(query.coord_sys);
    if (coord_result.IsError()) {
        return Result<MotionReadinessResult>::Failure(coord_result.GetError());
    }

    auto axes_result = motion_state_port_->GetAllAxesStatus();
    if (axes_result.IsError()) {
        return Result<MotionReadinessResult>::Failure(axes_result.GetError());
    }

    const auto& coord_status = coord_result.Value();
    const auto axis_statuses = axes_result.Value();
    const float32 velocity_tolerance = std::max(0.0f, query.velocity_tolerance_mm_s);

    if (IsBlockingJobState(query.active_job_state)) {
        return Result<MotionReadinessResult>::Success(MakeBlockedResult(
            coord_status,
            axis_statuses,
            "active_job_state=" + ToLower(query.active_job_state)));
    }

    if (coord_status.state == CoordinateSystemState::ERROR_STATE) {
        return Result<MotionReadinessResult>::Success(MakeBlockedResult(
            coord_status,
            axis_statuses,
            "coord_state=error"));
    }

    if (coord_status.state == CoordinateSystemState::MOVING ||
        coord_status.state == CoordinateSystemState::PAUSED ||
        coord_status.is_moving ||
        coord_status.remaining_segments != 0U ||
        std::abs(coord_status.current_velocity) > velocity_tolerance) {
        return Result<MotionReadinessResult>::Success(MakeBlockedResult(
            coord_status,
            axis_statuses,
            "coord_not_settled"));
    }

    for (const auto& axis_status : axis_statuses) {
        if (HasBlockingAxisState(axis_status) || HasVelocityAboveTolerance(axis_status, velocity_tolerance)) {
            return Result<MotionReadinessResult>::Success(MakeBlockedResult(
                coord_status,
                axis_statuses,
                "axis_not_settled"));
        }
    }

    MotionReadinessResult result;
    result.ready = true;
    result.coord_status = coord_status;
    result.axis_statuses = axis_statuses;
    result.message = "ready";
    return Result<MotionReadinessResult>::Success(result);
}

Result<MotionReadinessResult> MotionReadinessService::WaitUntilReady(
    const MotionReadinessQuery& query,
    int32 timeout_ms,
    int32 poll_interval_ms) const {
    if (timeout_ms <= 0 || poll_interval_ms <= 0) {
        return Result<MotionReadinessResult>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid readiness wait timing", kErrorSource));
    }

    MotionReadinessResult last_result;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    while (true) {
        auto readiness_result = Evaluate(query);
        if (readiness_result.IsError()) {
            return readiness_result;
        }

        last_result = readiness_result.Value();
        if (last_result.ready) {
            return Result<MotionReadinessResult>::Success(std::move(last_result));
        }

        if (std::chrono::steady_clock::now() >= deadline) {
            std::string timeout_message = last_result.message;
            if (!last_result.diagnostic_message.empty()) {
                if (!timeout_message.empty()) {
                    timeout_message += ";";
                }
                timeout_message += last_result.diagnostic_message;
            }
            return Result<MotionReadinessResult>::Failure(
                Error(
                    ErrorCode::MOTION_TIMEOUT,
                    timeout_message.empty() ? "motion_not_ready" : timeout_message,
                    kErrorSource));
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval_ms));
    }
}

}  // namespace Siligen::Application::Services::Motion::Execution
