#include "PlanningPreviewAssemblyService.h"

#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <limits>

namespace Siligen::Application::Services::Dispensing {

using Siligen::Application::UseCases::Dispensing::PlanningRequest;
using Siligen::Application::UseCases::Dispensing::PlanningResponse;
using Siligen::Domain::Dispensing::DomainServices::DispensingPlan;
using Siligen::Domain::Dispensing::DomainServices::DispensingPlanRequest;
using Siligen::Domain::Motion::ValueObjects::MotionTrajectory;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::VisualizationTriggerPoint;

namespace {

constexpr float32 kEpsilon = 1e-6f;

struct GluePointSegment {
    std::vector<Siligen::Shared::Types::Point2D> points;
};

struct PreviewTriggerConfig {
    float32 spatial_interval_mm = 1.0f;
};

struct SpacingStats {
    size_t count = 0;
    size_t within_tol = 0;
    size_t below_min = 0;
    size_t above_max = 0;
    float32 min_dist = std::numeric_limits<float32>::max();
    float32 max_dist = 0.0f;
};

struct ExecutionTrajectorySelection {
    std::vector<TrajectoryPoint> motion_trajectory_points;
    const std::vector<TrajectoryPoint>* execution_trajectory = nullptr;
};

SpacingStats EvaluateSpacing(const std::vector<GluePointSegment>& segments,
                             float32 target_mm,
                             float32 tol_ratio,
                             float32 min_allowed,
                             float32 max_allowed) {
    SpacingStats stats;
    if (target_mm <= kEpsilon) {
        return stats;
    }
    for (const auto& seg : segments) {
        if (seg.points.size() < 2) {
            continue;
        }
        for (size_t i = 1; i < seg.points.size(); ++i) {
            const float32 distance = seg.points[i - 1].DistanceTo(seg.points[i]);
            stats.count++;
            stats.min_dist = std::min(stats.min_dist, distance);
            stats.max_dist = std::max(stats.max_dist, distance);
            if (std::abs(distance - target_mm) <= target_mm * tol_ratio) {
                stats.within_tol++;
            }
            if (distance < min_allowed) {
                stats.below_min++;
            }
            if (distance > max_allowed) {
                stats.above_max++;
            }
        }
    }
    if (stats.count == 0) {
        stats.min_dist = 0.0f;
    }
    return stats;
}

std::vector<Siligen::Shared::Types::Point2D> BuildTriggerPointsFromDistances(
    const std::vector<TrajectoryPoint>& trajectory,
    const std::vector<float32>& distances_mm) {
    std::vector<Siligen::Shared::Types::Point2D> points;
    if (trajectory.size() < 2 || distances_mm.empty()) {
        return points;
    }

    std::vector<float32> cumulative(trajectory.size(), 0.0f);
    for (size_t i = 1; i < trajectory.size(); ++i) {
        cumulative[i] = cumulative[i - 1] + trajectory[i - 1].position.DistanceTo(trajectory[i].position);
    }

    std::vector<float32> sorted = distances_mm;
    std::sort(sorted.begin(), sorted.end());

    size_t idx = 1;
    for (const float32 target : sorted) {
        while (idx < cumulative.size() && cumulative[idx] + kEpsilon < target) {
            ++idx;
        }
        if (idx >= cumulative.size()) {
            break;
        }

        const float32 seg_len = cumulative[idx] - cumulative[idx - 1];
        const float32 ratio = (seg_len > kEpsilon) ? (target - cumulative[idx - 1]) / seg_len : 0.0f;
        const auto start = Siligen::Shared::Types::Point2D(trajectory[idx - 1].position);
        const auto end = Siligen::Shared::Types::Point2D(trajectory[idx].position);
        points.push_back(start + (end - start) * ratio);
    }

    return points;
}

std::vector<VisualizationTriggerPoint> BuildPlannedTriggerPoints(
    const std::vector<TrajectoryPoint>& trajectory,
    const PreviewTriggerConfig& config,
    const std::vector<float32>& trigger_distances_mm) {
    std::vector<VisualizationTriggerPoint> triggers;
    if (trajectory.size() < 2) {
        return triggers;
    }

    std::vector<Siligen::Shared::Types::Point2D> trigger_points;
    bool has_explicit = false;
    for (const auto& pt : trajectory) {
        if (pt.enable_position_trigger) {
            trigger_points.emplace_back(pt.position);
            has_explicit = true;
        }
    }

    if (!has_explicit) {
        if (!trigger_distances_mm.empty()) {
            trigger_points = BuildTriggerPointsFromDistances(trajectory, trigger_distances_mm);
        } else if (config.spatial_interval_mm > kEpsilon) {
            float32 accumulated = 0.0f;
            float32 next_trigger = config.spatial_interval_mm;
            for (size_t i = 1; i < trajectory.size(); ++i) {
                accumulated += trajectory[i - 1].position.DistanceTo(trajectory[i].position);
                if (accumulated + kEpsilon >= next_trigger) {
                    trigger_points.emplace_back(trajectory[i].position);
                    next_trigger += config.spatial_interval_mm;
                }
            }
        }
    }

    if (trigger_points.empty()) {
        return triggers;
    }

    triggers.reserve(trigger_points.size());
    const float32 nan_value = std::numeric_limits<float32>::quiet_NaN();
    for (const auto& pt : trigger_points) {
        triggers.emplace_back(pt, nan_value);
    }
    return triggers;
}

std::vector<TrajectoryPoint> BuildTrajectoryFromMotion(const MotionTrajectory& trajectory) {
    std::vector<TrajectoryPoint> base;
    if (trajectory.points.empty()) {
        return base;
    }
    base.reserve(trajectory.points.size());
    for (const auto& pt : trajectory.points) {
        TrajectoryPoint out;
        out.position = Siligen::Shared::Types::Point2D(pt.position);
        out.velocity = std::sqrt(pt.velocity.x * pt.velocity.x + pt.velocity.y * pt.velocity.y);
        out.timestamp = pt.t;
        out.enable_position_trigger = pt.dispense_on;
        base.push_back(out);
    }
    return base;
}

ExecutionTrajectorySelection SelectExecutionTrajectory(const DispensingPlan& plan) {
    ExecutionTrajectorySelection selection;
    selection.motion_trajectory_points = BuildTrajectoryFromMotion(plan.motion_trajectory);
    if (!plan.interpolation_points.empty()) {
        selection.execution_trajectory = &plan.interpolation_points;
        return selection;
    }
    selection.execution_trajectory = &selection.motion_trajectory_points;
    return selection;
}

std::vector<Siligen::Shared::Types::Point2D> CollectTriggerPositions(
    const std::vector<VisualizationTriggerPoint>& preview_triggers) {
    std::vector<Siligen::Shared::Types::Point2D> final_glue_points;
    final_glue_points.reserve(preview_triggers.size());
    for (const auto& tp : preview_triggers) {
        final_glue_points.push_back(tp.position);
    }
    return final_glue_points;
}

float32 ResolveTargetSpacing(const DispensingPlan& plan, const DispensingPlanRequest& plan_request) {
    if (plan.trigger_interval_mm > kEpsilon) {
        return plan.trigger_interval_mm;
    }
    if (plan_request.trigger_spatial_interval_mm > kEpsilon) {
        return plan_request.trigger_spatial_interval_mm;
    }
    return 0.0f;
}

void ValidateGlueSpacing(const PlanningRequest& request,
                         const std::vector<GluePointSegment>& glue_segments,
                         float32 target_spacing) {
    if (glue_segments.empty() || target_spacing <= kEpsilon) {
        return;
    }

    const float32 tol_ratio = (request.spacing_tol_ratio > 0.0f) ? request.spacing_tol_ratio : 0.1f;
    const float32 min_allowed = (request.spacing_min_mm > 0.0f) ? request.spacing_min_mm : 0.5f;
    const float32 max_allowed = (request.spacing_max_mm > 0.0f) ? request.spacing_max_mm : 10.0f;
    const auto stats = EvaluateSpacing(glue_segments, target_spacing, tol_ratio, min_allowed, max_allowed);
    if (stats.count == 0) {
        return;
    }

    const float32 within_ratio = static_cast<float32>(stats.within_tol) / static_cast<float32>(stats.count);
    if (within_ratio < 0.9f || stats.below_min > 0 || stats.above_max > 0) {
        SILIGEN_LOG_WARNING_FMT_HELPER(
            "Glue点距复核未达标: within=%.1f%% min=%.4f max=%.4f below=%zu above=%zu target=%.3f",
            within_ratio * 100.0f,
            stats.min_dist,
            stats.max_dist,
            stats.below_min,
            stats.above_max,
            target_spacing);
    }
}

float32 EstimateExecutionTime(const DispensingPlan& plan, const DispensingPlanRequest& plan_request) {
    float32 estimated_time = plan.estimated_time_s;
    if (estimated_time <= kEpsilon) {
        estimated_time = plan.motion_trajectory.total_time;
    }
    if (estimated_time <= kEpsilon &&
        plan.total_length_mm > kEpsilon &&
        plan_request.dispensing_velocity > kEpsilon) {
        estimated_time = plan.total_length_mm / plan_request.dispensing_velocity;
    }
    return estimated_time;
}

PlanningResponse BuildPlanningResponse(const DispensingPlan& plan,
                                       const std::vector<TrajectoryPoint>* execution_trajectory,
                                       const std::vector<Siligen::Shared::Types::Point2D>& final_glue_points,
                                       const std::string& dxf_filename,
                                       float32 estimated_time) {
    std::vector<TrajectoryPoint> preview_points;
    if (execution_trajectory) {
        preview_points = *execution_trajectory;
    }

    PlanningResponse response;
    response.success = true;
    response.segment_count = static_cast<int>(plan.process_path.segments.size());
    response.total_length = plan.total_length_mm > kEpsilon ? plan.total_length_mm : plan.motion_trajectory.total_length;
    response.estimated_time = estimated_time;
    response.trajectory_points = std::move(preview_points);
    response.process_tags.assign(response.trajectory_points.size(), 0);
    response.trigger_count = static_cast<int>(final_glue_points.size());
    response.dxf_filename = dxf_filename;
    response.timestamp = static_cast<int32>(std::time(nullptr));
    response.planning_report = plan.motion_trajectory.planning_report;
    return response;
}

PlanningArtifactExportRequest BuildExportRequest(const PlanningRequest& request,
                                                 const DispensingPlan& plan,
                                                 const ExecutionTrajectorySelection& selection,
                                                 const std::string& dxf_filename,
                                                 const std::vector<Siligen::Shared::Types::Point2D>& glue_points) {
    PlanningArtifactExportRequest export_request;
    export_request.source_path = request.dxf_filepath;
    export_request.dxf_filename = dxf_filename;
    export_request.process_path = plan.process_path;
    export_request.glue_points = glue_points;
    if (selection.execution_trajectory) {
        export_request.execution_trajectory_points = *selection.execution_trajectory;
    }
    export_request.interpolation_trajectory_points = plan.interpolation_points;
    export_request.motion_trajectory_points = selection.motion_trajectory_points;
    return export_request;
}

}  // namespace

Result<PlanningPreviewAssemblyResult> PlanningPreviewAssemblyService::BuildResponse(
    const PlanningRequest& request,
    const DispensingPlan& plan,
    const DispensingPlanRequest& plan_request,
    const std::string& dxf_filename) const {
    const auto selection = SelectExecutionTrajectory(plan);

    PreviewTriggerConfig trigger_config;
    trigger_config.spatial_interval_mm = plan_request.trigger_spatial_interval_mm;

    std::vector<VisualizationTriggerPoint> preview_triggers;
    if (selection.execution_trajectory) {
        preview_triggers = BuildPlannedTriggerPoints(
            *selection.execution_trajectory,
            trigger_config,
            plan.trigger_distances_mm);
    }
    if (preview_triggers.empty()) {
        return Result<PlanningPreviewAssemblyResult>::Failure(
            Error(ErrorCode::CMP_TRIGGER_SETUP_FAILED,
                  "位置触发不可用，禁止回退为定时触发",
                  "PlanningPreviewAssemblyService"));
    }

    const auto final_glue_points = CollectTriggerPositions(preview_triggers);
    const std::vector<GluePointSegment> glue_segments =
        final_glue_points.empty()
            ? std::vector<GluePointSegment>()
            : std::vector<GluePointSegment>{GluePointSegment{final_glue_points}};

    ValidateGlueSpacing(request, glue_segments, ResolveTargetSpacing(plan, plan_request));

    PlanningPreviewAssemblyResult result;
    result.response = BuildPlanningResponse(
        plan,
        selection.execution_trajectory,
        final_glue_points,
        dxf_filename,
        EstimateExecutionTime(plan, plan_request));
    result.export_request = BuildExportRequest(
        request,
        plan,
        selection,
        dxf_filename,
        final_glue_points);
    return Result<PlanningPreviewAssemblyResult>::Success(std::move(result));
}

}  // namespace Siligen::Application::Services::Dispensing
