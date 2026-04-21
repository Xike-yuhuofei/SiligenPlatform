#include "runtime_execution/application/services/motion/execution/MotionReadinessService.h"

#include "shared/types/Error.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <string_view>
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

ExecutionTransitionState ResolveActiveJobTransitionState(const MotionReadinessQuery& query) {
    if (query.active_job_transition_state != ExecutionTransitionState::UNSPECIFIED) {
        return query.active_job_transition_state;
    }
    return ParseExecutionTransitionState(query.active_job_state);
}

bool IsBlockingJobState(ExecutionTransitionState state) {
    return state == ExecutionTransitionState::RUNNING ||
           state == ExecutionTransitionState::AWAITING_CONTINUE ||
           state == ExecutionTransitionState::STOPPING ||
           state == ExecutionTransitionState::CANCELING ||
           state == ExecutionTransitionState::PAUSING ||
           state == ExecutionTransitionState::PAUSED;
}

std::string BuildActiveJobDiagnostic(
    const MotionReadinessQuery& query,
    ExecutionTransitionState state) {
    if (!query.active_job_state.empty()) {
        return "active_job_state=" + ToLower(query.active_job_state);
    }
    return std::string("active_job_state=") + ToString(state);
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
    MotionReadinessBlockCause block_cause,
    std::string diagnostic_message,
    ExecutionTransitionState active_job_transition_state = ExecutionTransitionState::UNSPECIFIED) {
    MotionReadinessResult result;
    result.ready = false;
    result.reason = MotionReadinessReason::MOTION_NOT_READY;
    result.block_cause = block_cause;
    result.active_job_transition_state = active_job_transition_state;
    result.reason_code = ToString(result.reason);
    result.message = result.reason_code;
    result.diagnostic_message = std::move(diagnostic_message);
    result.coord_status = coord_status;
    result.axis_statuses = std::move(axis_statuses);
    return result;
}

}  // namespace

const char* ToString(ExecutionTransitionState state) noexcept {
    switch (state) {
        case ExecutionTransitionState::PENDING:
            return "pending";
        case ExecutionTransitionState::RUNNING:
            return "running";
        case ExecutionTransitionState::AWAITING_CONTINUE:
            return "awaiting_continue";
        case ExecutionTransitionState::STOPPING:
            return "stopping";
        case ExecutionTransitionState::CANCELING:
            return "canceling";
        case ExecutionTransitionState::PAUSING:
            return "pausing";
        case ExecutionTransitionState::PAUSED:
            return "paused";
        case ExecutionTransitionState::COMPLETED:
            return "completed";
        case ExecutionTransitionState::CANCELLED:
            return "cancelled";
        case ExecutionTransitionState::FAILED:
            return "failed";
        case ExecutionTransitionState::IDLE:
            return "idle";
        case ExecutionTransitionState::UNKNOWN:
            return "unknown";
        case ExecutionTransitionState::UNSPECIFIED:
        default:
            return "unspecified";
    }
}

ExecutionTransitionState ParseExecutionTransitionState(std::string_view state) noexcept {
    const auto normalized = ToLower(std::string(state));
    if (normalized.empty()) {
        return ExecutionTransitionState::UNSPECIFIED;
    }
    if (normalized == "running") {
        return ExecutionTransitionState::RUNNING;
    }
    if (normalized == "pending") {
        return ExecutionTransitionState::PENDING;
    }
    if (normalized == "awaiting_continue") {
        return ExecutionTransitionState::AWAITING_CONTINUE;
    }
    if (normalized == "stopping") {
        return ExecutionTransitionState::STOPPING;
    }
    if (normalized == "canceling" || normalized == "cancelling" || normalized == "canceled") {
        return ExecutionTransitionState::CANCELING;
    }
    if (normalized == "paused") {
        return ExecutionTransitionState::PAUSED;
    }
    if (normalized == "pausing") {
        return ExecutionTransitionState::PAUSING;
    }
    if (normalized == "completed") {
        return ExecutionTransitionState::COMPLETED;
    }
    if (normalized == "cancelled") {
        return ExecutionTransitionState::CANCELLED;
    }
    if (normalized == "failed") {
        return ExecutionTransitionState::FAILED;
    }
    if (normalized == "idle") {
        return ExecutionTransitionState::IDLE;
    }
    return ExecutionTransitionState::UNKNOWN;
}

const char* ToString(MotionReadinessReason reason) noexcept {
    switch (reason) {
        case MotionReadinessReason::MOTION_NOT_READY:
            return "motion_not_ready";
        case MotionReadinessReason::NONE:
        default:
            return "";
    }
}

const char* ToString(MotionReadinessBlockCause cause) noexcept {
    switch (cause) {
        case MotionReadinessBlockCause::ACTIVE_JOB_STATE:
            return "active_job_state";
        case MotionReadinessBlockCause::COORDINATE_SYSTEM_ERROR:
            return "coord_state_error";
        case MotionReadinessBlockCause::COORDINATE_SYSTEM_NOT_SETTLED:
            return "coord_not_settled";
        case MotionReadinessBlockCause::AXIS_NOT_SETTLED:
            return "axis_not_settled";
        case MotionReadinessBlockCause::NONE:
        default:
            return "";
    }
}

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
    const auto active_job_transition_state = ResolveActiveJobTransitionState(query);

    if (IsBlockingJobState(active_job_transition_state)) {
        return Result<MotionReadinessResult>::Success(MakeBlockedResult(
            coord_status,
            axis_statuses,
            MotionReadinessBlockCause::ACTIVE_JOB_STATE,
            BuildActiveJobDiagnostic(query, active_job_transition_state),
            active_job_transition_state));
    }

    if (coord_status.state == CoordinateSystemState::ERROR_STATE) {
        return Result<MotionReadinessResult>::Success(MakeBlockedResult(
            coord_status,
            axis_statuses,
            MotionReadinessBlockCause::COORDINATE_SYSTEM_ERROR,
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
            MotionReadinessBlockCause::COORDINATE_SYSTEM_NOT_SETTLED,
            "coord_not_settled"));
    }

    for (const auto& axis_status : axis_statuses) {
        if (HasBlockingAxisState(axis_status) || HasVelocityAboveTolerance(axis_status, velocity_tolerance)) {
            return Result<MotionReadinessResult>::Success(MakeBlockedResult(
                coord_status,
                axis_statuses,
                MotionReadinessBlockCause::AXIS_NOT_SETTLED,
                "axis_not_settled"));
        }
    }

    MotionReadinessResult result;
    result.ready = true;
    result.reason = MotionReadinessReason::NONE;
    result.block_cause = MotionReadinessBlockCause::NONE;
    result.active_job_transition_state = active_job_transition_state;
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
