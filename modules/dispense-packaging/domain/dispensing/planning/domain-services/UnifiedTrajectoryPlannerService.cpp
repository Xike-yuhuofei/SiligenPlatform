#include "UnifiedTrajectoryPlannerService.h"

#include "process_path/contracts/GeometryUtils.h"
#include "shared/types/Error.h"

#include <cmath>
#include <sstream>
#include <utility>

namespace Siligen::Domain::Dispensing::DomainServices {

namespace {
constexpr float32 kEpsilon = 1e-6f;

using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
using Siligen::Application::Services::ProcessPath::ProcessPathBuildResult;
using Siligen::ProcessPath::Contracts::PathGenerationStage;
using Siligen::ProcessPath::Contracts::PathGenerationStatus;
using Siligen::ProcessPath::Contracts::SegmentEnd;
using Siligen::ProcessPath::Contracts::SegmentStart;
using Siligen::ProcessPath::Contracts::SegmentType;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Point2D;

void OptimizeLineOrientation(Siligen::ProcessPath::Contracts::Path& path, float32 tolerance) {
    if (path.segments.size() < 2) {
        return;
    }

    for (size_t i = 0; i < path.segments.size(); ++i) {
        auto& seg = path.segments[i];
        if (seg.type != SegmentType::Line) {
            continue;
        }

        const Point2D prev_end = (i > 0) ? SegmentEnd(path.segments[i - 1]) : seg.line.start;
        const Point2D next_start =
            (i + 1 < path.segments.size()) ? SegmentStart(path.segments[i + 1]) : seg.line.end;

        const Point2D start = seg.line.start;
        const Point2D end = seg.line.end;

        const float32 cost_current = prev_end.DistanceTo(start) + end.DistanceTo(next_start);
        const float32 cost_reversed = prev_end.DistanceTo(end) + start.DistanceTo(next_start);

        if (cost_reversed + std::max(tolerance, kEpsilon) < cost_current) {
            std::swap(seg.line.start, seg.line.end);
        }
    }
}

ProcessPathBuildRequest BuildProcessPathRequest(
    const std::vector<Siligen::ProcessPath::Contracts::Primitive>& primitives,
    const UnifiedTrajectoryPlanRequest& request) {
    ProcessPathBuildRequest build_request;
    build_request.primitives = primitives;
    build_request.normalization = request.normalization;
    build_request.process = request.process;
    build_request.shaping = request.shaping;
    build_request.topology_repair.policy = Siligen::ProcessPath::Contracts::TopologyRepairPolicy::Off;
    return build_request;
}

const char* StageToString(const PathGenerationStage stage) {
    switch (stage) {
        case PathGenerationStage::None:
            return "none";
        case PathGenerationStage::InputValidation:
            return "input_validation";
        case PathGenerationStage::TopologyRepair:
            return "topology_repair";
        case PathGenerationStage::Normalization:
            return "normalization";
        case PathGenerationStage::ProcessAnnotation:
            return "process_annotation";
        case PathGenerationStage::TrajectoryShaping:
            return "trajectory_shaping";
    }

    return "unknown";
}

Error BuildPathGenerationError(const ProcessPathBuildResult& build_result) {
    std::ostringstream oss;
    oss << "process path build failed at stage " << StageToString(build_result.failed_stage);
    if (!build_result.error_message.empty()) {
        oss << ": " << build_result.error_message;
    }

    const auto error_code = build_result.status == PathGenerationStatus::InvalidInput
        ? ErrorCode::INVALID_PARAMETER
        : ErrorCode::INVALID_STATE;
    return Error(error_code, oss.str(), "UnifiedTrajectoryPlannerService");
}

}  // namespace

UnifiedTrajectoryPlannerService::UnifiedTrajectoryPlannerService(
    std::shared_ptr<Domain::Motion::Ports::IVelocityProfilePort> velocity_profile_port)
    : velocity_profile_port_(std::move(velocity_profile_port)),
      motion_planning_facade_(velocity_profile_port_) {}

Siligen::Shared::Types::Result<UnifiedTrajectoryPlanResult> UnifiedTrajectoryPlannerService::Plan(
    const std::vector<Siligen::ProcessPath::Contracts::Primitive>& primitives,
    const UnifiedTrajectoryPlanRequest& request) const {
    const auto build_result = process_path_facade_.Build(BuildProcessPathRequest(primitives, request));
    if (build_result.status != PathGenerationStatus::Success) {
        return Siligen::Shared::Types::Result<UnifiedTrajectoryPlanResult>::Failure(
            BuildPathGenerationError(build_result));
    }

    UnifiedTrajectoryPlanResult result;
    result.normalized = build_result.normalized;
    OptimizeLineOrientation(result.normalized.path, request.normalization.continuity_tolerance);
    result.process_path = build_result.process_path;
    result.shaped_path = build_result.shaped_path;

    if (request.generate_motion_trajectory) {
        result.motion_trajectory = motion_planning_facade_.Plan(result.shaped_path, request.motion);
    }

    return Siligen::Shared::Types::Result<UnifiedTrajectoryPlanResult>::Success(std::move(result));
}

}  // namespace Siligen::Domain::Dispensing::DomainServices
