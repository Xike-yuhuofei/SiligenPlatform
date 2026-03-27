#include "PlanningPreviewAssemblyService.h"

#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
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

struct CsvWriteResult {
    bool ok = false;
    std::string path;
    std::string error;
    size_t point_count = 0;
};

struct DumpWriteResult {
    bool ok = false;
    std::string path;
    std::string error;
    size_t point_count = 0;
};

struct ABABacktrackStats {
    size_t count = 0;
    float32 max_distance = 0.0f;
};

struct PreviewTriggerConfig {
    float32 spatial_interval_mm = 1.0f;
};

const char* SegmentTypeLabel(Domain::Trajectory::ValueObjects::SegmentType type) {
    using Domain::Trajectory::ValueObjects::SegmentType;
    switch (type) {
        case SegmentType::Line:
            return "Line";
        case SegmentType::Arc:
            return "Arc";
        case SegmentType::Spline:
            return "Spline";
        default:
            return "Unknown";
    }
}

const char* ProcessTagLabel(Domain::Trajectory::ValueObjects::ProcessTag tag) {
    using Domain::Trajectory::ValueObjects::ProcessTag;
    switch (tag) {
        case ProcessTag::Normal:
            return "Normal";
        case ProcessTag::Start:
            return "Start";
        case ProcessTag::End:
            return "End";
        case ProcessTag::Corner:
            return "Corner";
        case ProcessTag::Rapid:
            return "Rapid";
        default:
            return "Unknown";
    }
}

Siligen::Shared::Types::Point2D ArcPoint(const Domain::Trajectory::ValueObjects::ArcPrimitive& arc,
                                         float32 angle_deg) {
    constexpr float32 kPi = 3.14159265359f;
    constexpr float32 kDegToRad = kPi / 180.0f;
    const float32 angle_rad = angle_deg * kDegToRad;
    return Siligen::Shared::Types::Point2D(
        arc.center.x + arc.radius * std::cos(angle_rad),
        arc.center.y + arc.radius * std::sin(angle_rad));
}

Siligen::Shared::Types::Point2D SegmentStart(const Domain::Trajectory::ValueObjects::Segment& segment) {
    using Domain::Trajectory::ValueObjects::SegmentType;
    switch (segment.type) {
        case SegmentType::Line:
            return segment.line.start;
        case SegmentType::Arc:
            return ArcPoint(segment.arc, segment.arc.start_angle_deg);
        case SegmentType::Spline:
            if (!segment.spline.control_points.empty()) {
                return segment.spline.control_points.front();
            }
            return Siligen::Shared::Types::Point2D();
        default:
            return Siligen::Shared::Types::Point2D();
    }
}

Siligen::Shared::Types::Point2D SegmentEnd(const Domain::Trajectory::ValueObjects::Segment& segment) {
    using Domain::Trajectory::ValueObjects::SegmentType;
    switch (segment.type) {
        case SegmentType::Line:
            return segment.line.end;
        case SegmentType::Arc:
            return ArcPoint(segment.arc, segment.arc.end_angle_deg);
        case SegmentType::Spline:
            if (!segment.spline.control_points.empty()) {
                return segment.spline.control_points.back();
            }
            return Siligen::Shared::Types::Point2D();
        default:
            return Siligen::Shared::Types::Point2D();
    }
}

std::string BuildGluePointsCsvPath(const std::string& dxf_path) {
    try {
        std::filesystem::path path(dxf_path);
        std::string stem = path.stem().string();
        if (stem.empty()) {
            stem = "glue_points";
        }
        const std::string filename = stem + "_glue_points.csv";
        if (path.has_parent_path()) {
            return (path.parent_path() / filename).string();
        }
        return filename;
    } catch (...) {
        return "glue_points.csv";
    }
}

std::string BuildTrajectoryDumpPath(const std::string& dxf_path,
                                    const std::string& tag,
                                    const std::string& ext,
                                    std::time_t timestamp) {
    try {
        std::filesystem::path path(dxf_path);
        std::string stem = path.stem().string();
        if (stem.empty()) {
            stem = "trajectory";
        }
        const std::string suffix = tag.empty() ? "trajectory" : (tag + "_trajectory");
        const std::string filename = stem + "_" + suffix + "_" + std::to_string(timestamp) + "." + ext;
        if (path.has_parent_path()) {
            return (path.parent_path() / filename).string();
        }
        return filename;
    } catch (...) {
        const std::string suffix = tag.empty() ? "trajectory" : (tag + "_trajectory");
        return "trajectory_" + suffix + "_" + std::to_string(timestamp) + "." + ext;
    }
}

struct SpacingStats {
    size_t count = 0;
    size_t within_tol = 0;
    size_t below_min = 0;
    size_t above_max = 0;
    float32 min_dist = std::numeric_limits<float32>::max();
    float32 max_dist = 0.0f;
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

ABABacktrackStats AnalyzeABABacktrack(const std::vector<TrajectoryPoint>& points,
                                      float32 pos_epsilon) {
    ABABacktrackStats stats;
    if (points.size() < 3) {
        return stats;
    }
    for (size_t i = 2; i < points.size(); ++i) {
        const auto& a = points[i - 2].position;
        const auto& b = points[i - 1].position;
        const auto& c = points[i].position;
        if (std::fabs(a.x - c.x) <= pos_epsilon && std::fabs(a.y - c.y) <= pos_epsilon) {
            const float32 dist = a.DistanceTo(b);
            stats.count += 1;
            if (dist > stats.max_distance) {
                stats.max_distance = dist;
            }
        }
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

CsvWriteResult WriteGluePointsCsv(const std::vector<GluePointSegment>& segments,
                                  const std::string& dxf_path) {
    CsvWriteResult result;
    result.path = BuildGluePointsCsvPath(dxf_path);

    try {
        std::filesystem::path path(result.path);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
    } catch (...) {
    }

    std::ofstream file(result.path, std::ios::binary);
    if (!file) {
        result.error = "Failed to open CSV file for writing";
        return result;
    }

    file.setf(std::ios::fixed);
    file << std::setprecision(6);
    file << "index,segment_index,segment_type,trigger_index,x_mm,y_mm\n";

    size_t global_index = 1;
    size_t trigger_index = 1;
    size_t segment_index = 0;
    for (const auto& segment : segments) {
        if (segment.points.empty()) {
            continue;
        }
        for (const auto& point : segment.points) {
            file << global_index << ','
                 << segment_index << ','
                 << "TRAJ" << ','
                 << trigger_index << ','
                 << point.x << ','
                 << point.y << '\n';
            ++global_index;
            ++trigger_index;
        }
        ++segment_index;
    }

    result.ok = true;
    result.point_count = global_index - 1;
    return result;
}

DumpWriteResult WriteTrajectoryPointsCsv(const std::vector<TrajectoryPoint>& points,
                                         const std::string& dxf_path,
                                         const std::string& tag,
                                         std::time_t timestamp) {
    DumpWriteResult result;
    result.path = BuildTrajectoryDumpPath(dxf_path, tag, "csv", timestamp);

    try {
        std::filesystem::path path(result.path);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
    } catch (...) {
    }

    std::ofstream file(result.path, std::ios::binary);
    if (!file) {
        result.error = "Failed to open trajectory CSV file for writing";
        return result;
    }

    file.setf(std::ios::fixed);
    file << std::setprecision(6);
    file << "index,x_mm,y_mm,velocity_mm_s,time_s,trigger\n";
    size_t index = 1;
    for (const auto& pt : points) {
        file << index << ','
             << pt.position.x << ','
             << pt.position.y << ','
             << pt.velocity << ','
             << pt.timestamp << ','
             << (pt.enable_position_trigger ? 1 : 0) << '\n';
        ++index;
    }

    result.ok = true;
    result.point_count = points.size();
    return result;
}

DumpWriteResult WriteProcessPathCsv(const Domain::Trajectory::ValueObjects::ProcessPath& path,
                                    const std::string& dxf_path,
                                    const std::string& tag,
                                    std::time_t timestamp) {
    DumpWriteResult result;
    result.path = BuildTrajectoryDumpPath(dxf_path, tag, "csv", timestamp);

    try {
        std::filesystem::path path_obj(result.path);
        if (path_obj.has_parent_path()) {
            std::filesystem::create_directories(path_obj.parent_path());
        }
    } catch (...) {
    }

    std::ofstream file(result.path, std::ios::binary);
    if (!file) {
        result.error = "Failed to open process path CSV file for writing";
        return result;
    }

    file.setf(std::ios::fixed);
    file << std::setprecision(6);
    file << "index,dispense_on,tag,type,start_x,start_y,end_x,end_y,length_mm\n";
    size_t index = 0;
    for (const auto& seg : path.segments) {
        const auto start = SegmentStart(seg.geometry);
        const auto end = SegmentEnd(seg.geometry);
        file << index << ','
             << (seg.dispense_on ? 1 : 0) << ','
             << ProcessTagLabel(seg.tag) << ','
             << SegmentTypeLabel(seg.geometry.type) << ','
             << start.x << ','
             << start.y << ','
             << end.x << ','
             << end.y << ','
             << seg.geometry.length << '\n';
        ++index;
    }

    result.ok = true;
    result.point_count = path.segments.size();
    return result;
}

DumpWriteResult WriteTrajectoryPointsJson(const std::vector<TrajectoryPoint>& points,
                                          const std::string& dxf_path,
                                          const std::string& tag,
                                          std::time_t timestamp) {
    DumpWriteResult result;
    result.path = BuildTrajectoryDumpPath(dxf_path, tag, "json", timestamp);

    try {
        std::filesystem::path path(result.path);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
    } catch (...) {
    }

    std::ofstream file(result.path, std::ios::binary);
    if (!file) {
        result.error = "Failed to open trajectory JSON file for writing";
        return result;
    }

    file.setf(std::ios::fixed);
    file << std::setprecision(6);
    file << "{\n";
    file << "  \"source\": \"" << tag << "\",\n";
    file << "  \"point_count\": " << points.size() << ",\n";
    file << "  \"points\": [\n";
    for (size_t i = 0; i < points.size(); ++i) {
        const auto& pt = points[i];
        file << "    [" << pt.position.x << ","
             << pt.position.y << ","
             << pt.velocity << ","
             << pt.timestamp << ","
             << (pt.enable_position_trigger ? 1 : 0) << "]";
        if (i + 1 < points.size()) {
            file << ",";
        }
        file << "\n";
    }
    file << "  ]\n";
    file << "}\n";

    result.ok = true;
    result.point_count = points.size();
    return result;
}

bool HasMonotonicTime(const std::vector<TrajectoryPoint>& points) {
    float32 last_time = -std::numeric_limits<float32>::infinity();
    bool has_time = false;
    for (const auto& pt : points) {
        if (pt.timestamp <= kEpsilon) {
            continue;
        }
        has_time = true;
        if (pt.timestamp + kEpsilon < last_time) {
            return false;
        }
        last_time = pt.timestamp;
    }
    return has_time;
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

struct ExecutionTrajectorySelection {
    std::vector<TrajectoryPoint> motion_trajectory_points;
    const std::vector<TrajectoryPoint>* execution_trajectory = nullptr;
};

ExecutionTrajectorySelection SelectExecutionTrajectory(const DispensingPlan& plan, bool dump_preview) {
    ExecutionTrajectorySelection selection;
    if (!plan.interpolation_points.empty()) {
        selection.execution_trajectory = &plan.interpolation_points;
        if (dump_preview) {
            selection.motion_trajectory_points = BuildTrajectoryFromMotion(plan.motion_trajectory);
        }
        return selection;
    }

    selection.motion_trajectory_points = BuildTrajectoryFromMotion(plan.motion_trajectory);
    selection.execution_trajectory = &selection.motion_trajectory_points;
    return selection;
}

void LogDumpResult(const char* success_label,
                   const char* failure_label,
                   const DumpWriteResult& result) {
    if (result.ok) {
        SILIGEN_LOG_INFO_FMT_HELPER("%s: path=%s, points=%zu",
                                    success_label,
                                    result.path.c_str(),
                                    result.point_count);
    } else {
        SILIGEN_LOG_WARNING(std::string(failure_label) + result.error);
    }
}

void DumpTrajectoryArtifacts(const std::vector<TrajectoryPoint>& points,
                             const std::string& dxf_filepath,
                             const char* tag,
                             std::time_t dump_timestamp,
                             const char* stats_label,
                             const char* csv_success_label,
                             const char* csv_failure_label,
                             const char* json_success_label,
                             const char* json_failure_label) {
    const ABABacktrackStats stats = AnalyzeABABacktrack(points, 1e-4f);
    SILIGEN_LOG_INFO_FMT_HELPER(stats_label,
                                points.size(),
                                stats.count,
                                stats.max_distance,
                                HasMonotonicTime(points) ? "true" : "false");

    const auto csv_result = WriteTrajectoryPointsCsv(points, dxf_filepath, tag, dump_timestamp);
    const auto json_result = WriteTrajectoryPointsJson(points, dxf_filepath, tag, dump_timestamp);
    LogDumpResult(csv_success_label, csv_failure_label, csv_result);
    LogDumpResult(json_success_label, json_failure_label, json_result);
}

void DumpPreviewArtifacts(const PlanningRequest& request,
                          const DispensingPlan& plan,
                          const ExecutionTrajectorySelection& selection,
                          bool dump_preview,
                          std::time_t dump_timestamp) {
    if (!dump_preview) {
        return;
    }

    const auto process_csv = WriteProcessPathCsv(plan.process_path, request.dxf_filepath, "process_path", dump_timestamp);
    if (process_csv.ok) {
        SILIGEN_LOG_INFO_FMT_HELPER("Process path CSV written: path=%s, segments=%zu",
                                    process_csv.path.c_str(),
                                    process_csv.point_count);
    } else {
        SILIGEN_LOG_WARNING("Process path CSV failed: " + process_csv.error);
    }

    if (selection.execution_trajectory && !selection.execution_trajectory->empty()) {
        DumpTrajectoryArtifacts(*selection.execution_trajectory,
                                request.dxf_filepath,
                                "execution",
                                dump_timestamp,
                                "Preview trajectory dump: source=execution points=%zu aba=%zu max_aba=%.6f monotonic=%s",
                                "Preview trajectory CSV written",
                                "Preview trajectory CSV failed: ",
                                "Preview trajectory JSON written",
                                "Preview trajectory JSON failed: ");
    }

    if (!plan.interpolation_points.empty()) {
        DumpTrajectoryArtifacts(plan.interpolation_points,
                                request.dxf_filepath,
                                "interpolation",
                                dump_timestamp,
                                "Preview trajectory stats: source=interpolation points=%zu aba=%zu max_aba=%.6f monotonic=%s",
                                "Interpolation trajectory CSV written",
                                "Interpolation trajectory CSV failed: ",
                                "Interpolation trajectory JSON written",
                                "Interpolation trajectory JSON failed: ");
    }

    if (!selection.motion_trajectory_points.empty() &&
        selection.execution_trajectory != &selection.motion_trajectory_points) {
        DumpTrajectoryArtifacts(selection.motion_trajectory_points,
                                request.dxf_filepath,
                                "motion",
                                dump_timestamp,
                                "Preview trajectory stats: source=motion points=%zu aba=%zu max_aba=%.6f monotonic=%s",
                                "Motion trajectory CSV written",
                                "Motion trajectory CSV failed: ",
                                "Motion trajectory JSON written",
                                "Motion trajectory JSON failed: ");
    }
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

void WriteGluePointArtifacts(const std::vector<GluePointSegment>& glue_segments, const std::string& dxf_filepath) {
    if (glue_segments.empty()) {
        return;
    }

    const auto csv_result = WriteGluePointsCsv(glue_segments, dxf_filepath);
    if (csv_result.ok) {
        SILIGEN_LOG_INFO_FMT_HELPER("Glue points CSV written: path=%s, points=%zu",
                                    csv_result.path.c_str(),
                                    csv_result.point_count);
    } else {
        SILIGEN_LOG_WARNING("Failed to write glue points CSV: " + csv_result.error);
    }
}

float32 EstimateExecutionTime(const DispensingPlan& plan, const DispensingPlanRequest& plan_request) {
    float32 estimated_time = plan.estimated_time_s;
    if (estimated_time <= kEpsilon) {
        estimated_time = plan.motion_trajectory.total_time;
    }
    if (estimated_time <= kEpsilon && plan.total_length_mm > kEpsilon && plan_request.dispensing_velocity > kEpsilon) {
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

}  // namespace

Result<PlanningResponse> PlanningPreviewAssemblyService::BuildResponse(
    const PlanningRequest& request,
    const DispensingPlan& plan,
    const DispensingPlanRequest& plan_request,
    bool dump_preview,
    std::time_t dump_timestamp,
    const std::string& dxf_filename) const {
    const auto selection = SelectExecutionTrajectory(plan, dump_preview);
    DumpPreviewArtifacts(request, plan, selection, dump_preview, dump_timestamp);

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
        return Result<PlanningResponse>::Failure(
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
    WriteGluePointArtifacts(glue_segments, request.dxf_filepath);

    return Result<PlanningResponse>::Success(
        BuildPlanningResponse(
            plan,
            selection.execution_trajectory,
            final_glue_points,
            dxf_filename,
            EstimateExecutionTime(plan, plan_request)));
}

}  // namespace Siligen::Application::Services::Dispensing
