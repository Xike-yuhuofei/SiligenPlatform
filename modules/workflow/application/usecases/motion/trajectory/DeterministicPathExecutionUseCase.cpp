#include "DeterministicPathExecutionUseCase.h"

#include "domain/motion/domain-services/interpolation/InterpolationProgramPlanner.h"
#include "domain/motion/domain-services/MotionPlanner.h"
#include "motion_planning/contracts/TimePlanningConfig.h"
#include "process_path/contracts/ProcessPath.h"
#include "process_path/contracts/GeometryUtils.h"
#include "shared/types/Error.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace Siligen::Application::UseCases::Motion::Trajectory {

using Coordination::MotionCoordinationUseCase;
using Siligen::Domain::Motion::DomainServices::InterpolationProgramPlanner;
using Siligen::Domain::Motion::DomainServices::MotionPlanner;
using Siligen::RuntimeExecution::Contracts::Motion::CoordinateSystemStatus;
using Siligen::RuntimeExecution::Contracts::Motion::InterpolationData;
using Siligen::Domain::Motion::Ports::MotionStatus;
using Siligen::Domain::Motion::Ports::MotionState;
using Siligen::MotionPlanning::Contracts::TimePlanningConfig;
using Siligen::ProcessPath::Contracts::ProcessPath;
using Siligen::ProcessPath::Contracts::ProcessSegment;
using Siligen::ProcessPath::Contracts::ProcessTag;
using Siligen::ProcessPath::Contracts::SegmentType;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;

namespace {

constexpr double kPi = 3.14159265358979323846;

Error MakeError(ErrorCode code, const std::string& message, const char* source) {
    return Error(code, message, source);
}

bool IsFinitePositive(float32 value) {
    return std::isfinite(value) && value > 0.0f;
}

bool IsBenignCoordinateSystemStopError(const Error& error) {
    return error.GetCode() == ErrorCode::INVALID_PARAMETER &&
        error.GetMessage().find("Coordinate system not configured") != std::string::npos;
}

std::uint32_t CoordinateSystemMask(int16 coord_sys) {
    return static_cast<std::uint32_t>(1u << static_cast<unsigned>(coord_sys - 1));
}

}  // namespace

bool DeterministicPathExecutionRequest::Validate() const noexcept {
    if (segments.empty() || coord_sys <= 0) {
        return false;
    }
    if (!IsFinitePositive(max_velocity_mm_s) ||
        !IsFinitePositive(max_acceleration_mm_s2) ||
        !IsFinitePositive(sample_dt_s) ||
        axis_map.empty()) {
        return false;
    }

    bool has_x = false;
    bool has_y = false;
    for (const auto axis : axis_map) {
        if (axis == LogicalAxisId::X) {
            has_x = true;
            continue;
        }
        if (axis == LogicalAxisId::Y) {
            has_y = true;
            continue;
        }
        return false;
    }

    return has_x && has_y;
}

DeterministicPathExecutionUseCase::DeterministicPathExecutionUseCase(
    std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IInterpolationPort> interpolation_port,
    std::shared_ptr<Domain::Motion::Ports::IMotionStatePort> motion_state_port)
    : interpolation_port_(std::move(interpolation_port)),
      motion_state_port_(std::move(motion_state_port)),
      coordination_use_case_(interpolation_port_, nullptr) {}

Result<DeterministicPathExecutionStatus> DeterministicPathExecutionUseCase::Start(
    const DeterministicPathExecutionRequest& request) {
    if (!interpolation_port_ || !motion_state_port_) {
        return Result<DeterministicPathExecutionStatus>::Failure(
            MakeError(
                ErrorCode::PORT_NOT_INITIALIZED,
                "Deterministic path execution ports are not initialized",
                "DeterministicPathExecutionUseCase::Start"));
    }
    if (HasActiveExecution()) {
        return Result<DeterministicPathExecutionStatus>::Failure(
            MakeError(
                ErrorCode::INVALID_STATE,
                "Deterministic path execution already has active work",
                "DeterministicPathExecutionUseCase::Start"));
    }
    if (!request.Validate()) {
        return Result<DeterministicPathExecutionStatus>::Failure(
            MakeError(
                ErrorCode::INVALID_PARAMETER,
                "Deterministic path execution request is invalid",
                "DeterministicPathExecutionUseCase::Start"));
    }

    const auto position_result = motion_state_port_->GetCurrentPosition();
    if (position_result.IsError()) {
        return Result<DeterministicPathExecutionStatus>::Failure(position_result.GetError());
    }

    auto execution_result = BuildExecution(request, position_result.Value());
    if (execution_result.IsError()) {
        return Result<DeterministicPathExecutionStatus>::Failure(execution_result.GetError());
    }

    const auto configure_result = coordination_use_case_.ConfigureCoordinateSystem(
        request.coord_sys,
        request.axis_map,
        request.max_velocity_mm_s);
    if (configure_result.IsError()) {
        return Result<DeterministicPathExecutionStatus>::Failure(configure_result.GetError());
    }

    active_execution_ = execution_result.Value();
    const auto clear_result = interpolation_port_->ClearInterpolationBuffer(request.coord_sys);
    if (clear_result.IsError()) {
        active_execution_.reset();
        return Result<DeterministicPathExecutionStatus>::Failure(clear_result.GetError());
    }

    status_.state = DeterministicPathExecutionState::READY;
    status_.total_segments = static_cast<int32>(active_execution_->program.size());
    status_.dispatched_segments = 0;
    status_.segment_in_flight = false;
    status_.detail = "Deterministic path execution accepted for tick-driven advancement.";
    return Result<DeterministicPathExecutionStatus>::Success(status_);
}

Result<DeterministicPathExecutionStatus> DeterministicPathExecutionUseCase::Advance() {
    if (!active_execution_.has_value()) {
        if (status_.state == DeterministicPathExecutionState::COMPLETED ||
            status_.state == DeterministicPathExecutionState::FAILED ||
            status_.state == DeterministicPathExecutionState::CANCELLED) {
            return Result<DeterministicPathExecutionStatus>::Success(status_);
        }

        return Result<DeterministicPathExecutionStatus>::Failure(
            MakeError(
                ErrorCode::INVALID_STATE,
                "No active deterministic path execution",
                "DeterministicPathExecutionUseCase::Advance"));
    }

    auto statuses_result = ReadMotionStatuses();
    if (statuses_result.IsError()) {
        const auto detail = statuses_result.GetError().GetMessage();
        (void)FailActiveExecution(detail);
        return Result<DeterministicPathExecutionStatus>::Failure(statuses_result.GetError());
    }

    const auto& statuses = statuses_result.Value();
    for (const auto& status : statuses) {
        if (status.has_error) {
            const std::string detail =
                "Motion state reported axis error code " + std::to_string(status.error_code) + ".";
            (void)FailActiveExecution(detail);
            return Result<DeterministicPathExecutionStatus>::Failure(
                MakeError(
                    ErrorCode::HARDWARE_ERROR,
                    detail,
                    "DeterministicPathExecutionUseCase::Advance"));
        }
    }

    auto& execution = *active_execution_;
    const auto start_result = coordination_use_case_.StartCoordinateSystemMotion(
        CoordinateSystemMask(execution.coord_sys));
    if (start_result.IsError() && !IsBenignCoordinateSystemStopError(start_result.GetError())) {
        const auto detail = start_result.GetError().GetMessage();
        (void)FailActiveExecution(detail);
        return Result<DeterministicPathExecutionStatus>::Failure(start_result.GetError());
    }

    auto coord_status_result = ReadCoordinateSystemStatus(execution.coord_sys);
    if (coord_status_result.IsError()) {
        const auto detail = coord_status_result.GetError().GetMessage();
        (void)FailActiveExecution(detail);
        return Result<DeterministicPathExecutionStatus>::Failure(coord_status_result.GetError());
    }

    const auto buffer_space_result = interpolation_port_->GetInterpolationBufferSpace(execution.coord_sys);
    if (buffer_space_result.IsError()) {
        const auto detail = buffer_space_result.GetError().GetMessage();
        (void)FailActiveExecution(detail);
        return Result<DeterministicPathExecutionStatus>::Failure(buffer_space_result.GetError());
    }

    const auto lookahead_space_result = interpolation_port_->GetLookAheadBufferSpace(execution.coord_sys);
    if (lookahead_space_result.IsError()) {
        const auto detail = lookahead_space_result.GetError().GetMessage();
        (void)FailActiveExecution(detail);
        return Result<DeterministicPathExecutionStatus>::Failure(lookahead_space_result.GetError());
    }

    auto coord_status = coord_status_result.Value();
    const auto allowed_prefetch = std::min<uint32_t>(
        buffer_space_result.Value(),
        static_cast<uint32_t>((coord_status.remaining_segments == 0U ? 1U : 0U) + lookahead_space_result.Value()));

    std::size_t prefetched_segments = 0U;
    while (prefetched_segments < allowed_prefetch && execution.next_segment_index < execution.program.size()) {
        const auto dispatch_result = DispatchNextSegment(execution);
        if (dispatch_result.IsError()) {
            const auto detail = dispatch_result.GetError().GetMessage();
            (void)FailActiveExecution(detail);
            return Result<DeterministicPathExecutionStatus>::Failure(dispatch_result.GetError());
        }
        ++prefetched_segments;
    }

    coord_status_result = ReadCoordinateSystemStatus(execution.coord_sys);
    if (coord_status_result.IsError()) {
        const auto detail = coord_status_result.GetError().GetMessage();
        (void)FailActiveExecution(detail);
        return Result<DeterministicPathExecutionStatus>::Failure(coord_status_result.GetError());
    }
    coord_status = coord_status_result.Value();

    status_.dispatched_segments = static_cast<int32>(execution.next_segment_index);
    status_.segment_in_flight = coord_status.is_moving || coord_status.remaining_segments > 0U;
    if (execution.next_segment_index >= execution.program.size() && !status_.segment_in_flight) {
        active_execution_.reset();
        status_.state = DeterministicPathExecutionState::COMPLETED;
        status_.detail = "Deterministic path execution completed.";
        return Result<DeterministicPathExecutionStatus>::Success(status_);
    }

    status_.state = status_.segment_in_flight || execution.next_segment_index > 0U
        ? DeterministicPathExecutionState::RUNNING
        : DeterministicPathExecutionState::READY;
    if (prefetched_segments > 0U) {
        status_.detail =
            "Buffered path segments through " + std::to_string(status_.dispatched_segments) + "/" +
            std::to_string(status_.total_segments) + ".";
    } else if (coord_status.remaining_segments > 0U) {
        status_.detail =
            "Waiting for buffered path segments to drain (" +
            std::to_string(coord_status.remaining_segments) + " remaining).";
    } else {
        status_.detail = "Waiting for interpolation buffer space.";
    }
    return Result<DeterministicPathExecutionStatus>::Success(status_);
}

DeterministicPathExecutionStatus DeterministicPathExecutionUseCase::Status() const noexcept {
    return status_;
}

Result<void> DeterministicPathExecutionUseCase::Cancel() {
    if (!active_execution_.has_value()) {
        return Result<void>::Success();
    }

    const auto stop_result = coordination_use_case_.StopCoordinateSystemMotion(
        CoordinateSystemMask(active_execution_->coord_sys));
    active_execution_.reset();
    status_.state = DeterministicPathExecutionState::CANCELLED;
    status_.segment_in_flight = false;
    status_.detail = "Deterministic path execution cancelled.";

    if (stop_result.IsError() && !IsBenignCoordinateSystemStopError(stop_result.GetError())) {
        return Result<void>::Failure(stop_result.GetError());
    }

    return Result<void>::Success();
}

bool DeterministicPathExecutionUseCase::HasActiveExecution() const noexcept {
    return active_execution_.has_value();
}

Result<DeterministicPathExecutionUseCase::ActiveExecution> DeterministicPathExecutionUseCase::BuildExecution(
    const DeterministicPathExecutionRequest& request,
    const Point2D& start_point) const {
    MotionPlanner motion_planner;
    InterpolationProgramPlanner program_planner;

    ProcessPath path;
    path.segments.reserve(request.segments.size());

    Point2D cursor = start_point;
    for (std::size_t index = 0; index < request.segments.size(); ++index) {
        const auto& segment_request = request.segments[index];
        ProcessSegment segment;
        segment.tag = ProcessTag::Normal;
        if (request.segments.size() > 1U) {
            if (index == 0U) {
                segment.tag = ProcessTag::Start;
            } else if (index + 1U == request.segments.size()) {
                segment.tag = ProcessTag::End;
            }
        }

        if (segment_request.type == DeterministicPathSegmentType::LINE) {
            segment.geometry.type = SegmentType::Line;
            segment.geometry.line.start = cursor;
            segment.geometry.line.end = segment_request.end_point;
            segment.geometry.length = cursor.DistanceTo(segment_request.end_point);
        } else {
            const auto radius = cursor.DistanceTo(segment_request.center_point);
            if (radius <= 0.0f) {
                return Result<ActiveExecution>::Failure(
                    MakeError(
                        ErrorCode::INVALID_PARAMETER,
                        "Arc segment radius must be positive",
                        "DeterministicPathExecutionUseCase::BuildExecution"));
            }

            segment.geometry.type = SegmentType::Arc;
            segment.geometry.arc.center = segment_request.center_point;
            segment.geometry.arc.radius = radius;
            segment.geometry.arc.start_angle_deg = static_cast<float32>(
                std::atan2(cursor.y - segment_request.center_point.y, cursor.x - segment_request.center_point.x) *
                180.0 / kPi);
            segment.geometry.arc.end_angle_deg = static_cast<float32>(
                std::atan2(
                    segment_request.end_point.y - segment_request.center_point.y,
                    segment_request.end_point.x - segment_request.center_point.x) *
                180.0 / kPi);
            segment.geometry.arc.clockwise = segment_request.type == DeterministicPathSegmentType::ARC_CW;
            segment.geometry.length = ComputeArcLength(segment.geometry.arc);

            if (segment.geometry.length <= 0.0f) {
                return Result<ActiveExecution>::Failure(
                    MakeError(
                        ErrorCode::INVALID_PARAMETER,
                        "Arc segment length must be positive",
                        "DeterministicPathExecutionUseCase::BuildExecution"));
            }
        }

        path.segments.push_back(segment);
        cursor = segment_request.end_point;
    }

    TimePlanningConfig config;
    config.vmax = request.max_velocity_mm_s;
    config.amax = request.max_acceleration_mm_s2;
    config.sample_dt = request.sample_dt_s;
    config.enforce_jerk_limit = false;

    const auto trajectory = motion_planner.Plan(path, config);
    auto program_result = program_planner.BuildProgram(path, trajectory, config.amax);
    if (program_result.IsError()) {
        return Result<ActiveExecution>::Failure(program_result.GetError());
    }

    ActiveExecution execution;
    execution.program = program_result.Value();
    execution.axis_map = request.axis_map;
    execution.coord_sys = request.coord_sys;
    return Result<ActiveExecution>::Success(std::move(execution));
}

Result<void> DeterministicPathExecutionUseCase::DispatchNextSegment(ActiveExecution& execution) {
    if (execution.next_segment_index >= execution.program.size()) {
        return Result<void>::Failure(
            MakeError(
                ErrorCode::INVALID_STATE,
                "No remaining path segment to dispatch",
                "DeterministicPathExecutionUseCase::DispatchNextSegment"));
    }

    const auto& raw_segment = execution.program[execution.next_segment_index];
    auto mapped_segment = raw_segment;
    mapped_segment.positions.clear();
    mapped_segment.positions.reserve(execution.axis_map.size());

    for (const auto axis : execution.axis_map) {
        if (axis == LogicalAxisId::X) {
            mapped_segment.positions.push_back(raw_segment.positions.size() > 0 ? raw_segment.positions[0] : 0.0f);
        } else if (axis == LogicalAxisId::Y) {
            mapped_segment.positions.push_back(raw_segment.positions.size() > 1 ? raw_segment.positions[1] : 0.0f);
        } else {
            return Result<void>::Failure(
                MakeError(
                    ErrorCode::INVALID_PARAMETER,
                    "Unsupported logical axis in deterministic path axis map",
                    "DeterministicPathExecutionUseCase::DispatchNextSegment"));
        }
    }

    const auto dispatch_result = coordination_use_case_.DispatchCoordinateSystemSegment(
        execution.coord_sys,
        mapped_segment);
    if (dispatch_result.IsError()) {
        return dispatch_result;
    }

    ++execution.next_segment_index;
    return Result<void>::Success();
}

Result<CoordinateSystemStatus> DeterministicPathExecutionUseCase::ReadCoordinateSystemStatus(
    int16 coord_sys) const {
    if (!interpolation_port_) {
        return Result<CoordinateSystemStatus>::Failure(
            MakeError(
                ErrorCode::PORT_NOT_INITIALIZED,
                "Interpolation port is not initialized",
                "DeterministicPathExecutionUseCase::ReadCoordinateSystemStatus"));
    }

    return interpolation_port_->GetCoordinateSystemStatus(coord_sys);
}

Result<std::vector<MotionStatus>> DeterministicPathExecutionUseCase::ReadMotionStatuses() const {
    if (!motion_state_port_) {
        return Result<std::vector<MotionStatus>>::Failure(
            MakeError(
                ErrorCode::PORT_NOT_INITIALIZED,
                "Motion state port is not initialized",
                "DeterministicPathExecutionUseCase::ReadMotionStatuses"));
    }
    return motion_state_port_->GetAllAxesStatus();
}

Result<void> DeterministicPathExecutionUseCase::FailActiveExecution(const std::string& detail) {
    const auto coord_sys = active_execution_.has_value() ? active_execution_->coord_sys : static_cast<int16>(0);
    active_execution_.reset();
    status_.state = DeterministicPathExecutionState::FAILED;
    status_.segment_in_flight = false;
    status_.detail = detail;

    if (coord_sys > 0) {
        const auto stop_result = coordination_use_case_.StopCoordinateSystemMotion(
            CoordinateSystemMask(coord_sys));
        if (stop_result.IsError() && !IsBenignCoordinateSystemStopError(stop_result.GetError())) {
            return Result<void>::Failure(stop_result.GetError());
        }
    }

    return Result<void>::Success();
}

}  // namespace Siligen::Application::UseCases::Motion::Trajectory
