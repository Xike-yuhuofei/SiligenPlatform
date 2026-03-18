#include "DXFWebPlanningUseCase.h"
#include "application/services/dxf/DxfPbPreparationService.h"
#include "shared/interfaces/ILoggingService.h"
#include "shared/logging/PrintfLogFormatter.h"
#include "domain/trajectory/value-objects/ProcessPath.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <utility>

#ifdef MODULE_NAME
#undef MODULE_NAME
#endif
#define MODULE_NAME "DXFWebPlanningUseCase"

namespace Siligen::Application::UseCases::Dispensing::DXF {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;
using Siligen::Shared::Types::VisualizationTriggerPoint;
using Siligen::Domain::Motion::ValueObjects::MotionTrajectory;

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
    float32 angle_rad = angle_deg * kDegToRad;
    return Siligen::Shared::Types::Point2D(arc.center.x + arc.radius * std::cos(angle_rad),
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

struct PreviewRuntimeParams {
    float32 dispensing_velocity = 0.0f;
    float32 acceleration = 0.0f;
    float32 pulse_per_mm = 200.0f;
    uint32 dispenser_interval_ms = 0;
    uint32 dispenser_duration_ms = 0;
    float32 trigger_spatial_interval_mm = 0.0f;
    float32 valve_response_ms = 0.0f;
    float32 safety_margin_ms = 0.0f;
    float32 min_interval_ms = 0.0f;
    float32 sample_dt = 0.01f;
    float32 sample_ds = 0.0f;
    Siligen::Domain::Dispensing::ValueObjects::DispenseCompensationProfile compensation_profile{};
};

struct PreviewTriggerConfig {
    float32 spatial_interval_mm = 1.0f;
};

bool HasMonotonicTime(const std::vector<TrajectoryPoint>& points);

bool IsTruthyEnv(const char* value) {
    if (!value || *value == '\0') {
        return false;
    }
    std::string normalized(value);
    for (auto& c : normalized) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
}

bool ShouldDumpPreviewTrajectory() {
    return IsTruthyEnv(std::getenv("SILIGEN_DUMP_PREVIEW_TRAJECTORY"));
}

std::string BuildGluePointsCsvPath(const std::string& dxf_path) {
    try {
        std::filesystem::path path(dxf_path);
        std::string stem = path.stem().string();
        if (stem.empty()) {
            stem = "glue_points";
        }
        std::string filename = stem + "_glue_points.csv";
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
        std::string suffix = tag.empty() ? "trajectory" : (tag + "_trajectory");
        std::string filename = stem + "_" + suffix + "_" + std::to_string(timestamp) + "." + ext;
        if (path.has_parent_path()) {
            return (path.parent_path() / filename).string();
        }
        return filename;
    } catch (...) {
        std::string suffix = tag.empty() ? "trajectory" : (tag + "_trajectory");
        return "trajectory_" + suffix + "_" + std::to_string(timestamp) + "." + ext;
    }
}

float32 Clamp(float32 value, float32 lo, float32 hi) {
    return std::min(std::max(value, lo), hi);
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
            float32 d = seg.points[i - 1].DistanceTo(seg.points[i]);
            stats.count++;
            stats.min_dist = std::min(stats.min_dist, d);
            stats.max_dist = std::max(stats.max_dist, d);
            if (std::abs(d - target_mm) <= target_mm * tol_ratio) {
                stats.within_tol++;
            }
            if (d < min_allowed) {
                stats.below_min++;
            }
            if (d > max_allowed) {
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

PreviewRuntimeParams BuildPreviewRuntimeParams(
    const DXFPlanningRequest& request,
    const std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort>& config_port) {
    PreviewRuntimeParams params;
    params.dispensing_velocity = request.trajectory_config.max_velocity;
    params.acceleration = request.trajectory_config.max_acceleration;
    params.sample_dt = request.trajectory_config.time_step;
    params.sample_ds = 0.0f;
    params.dispenser_duration_ms = static_cast<uint32>(std::max(
        1.0f,
        static_cast<float32>(request.trajectory_config.trigger_pulse_us) / 1000.0f));

    float32 safety_limit_velocity = 0.0f;
    if (config_port) {
        auto config_result = config_port->LoadConfiguration();
        if (config_result.IsSuccess()) {
            const auto& config = config_result.Value();
            if (config.machine.max_speed > 0.0f) {
                safety_limit_velocity = config.machine.max_speed;
            }
            if (config.machine.max_acceleration > 0.0f) {
                params.acceleration = config.machine.max_acceleration;
            }
            if (config.machine.pulse_per_mm > 0.0f) {
                params.pulse_per_mm = config.machine.pulse_per_mm;
            }
            if (config.dispensing.dot_diameter_target_mm > 0.0f) {
                float32 interval = config.dispensing.dot_diameter_target_mm;
                if (config.dispensing.dot_edge_gap_mm > 0.0f) {
                    interval += config.dispensing.dot_edge_gap_mm;
                }
                params.trigger_spatial_interval_mm = interval;
            }
            params.compensation_profile.open_comp_ms = config.dispensing.open_comp_ms;
            params.compensation_profile.close_comp_ms = config.dispensing.close_comp_ms;
            params.compensation_profile.retract_enabled = config.dispensing.retract_enabled;
            params.compensation_profile.corner_pulse_scale = config.dispensing.corner_pulse_scale;
            params.compensation_profile.curvature_speed_factor = config.dispensing.curvature_speed_factor;
            params.sample_dt = config.dispensing.trajectory_sample_dt;
            params.sample_ds = config.dispensing.trajectory_sample_ds;
        } else {
            SILIGEN_LOG_WARNING("读取系统配置失败: " + config_result.GetError().GetMessage());
        }

        auto valve_coord_result = config_port->GetValveCoordinationConfig();
        if (valve_coord_result.IsSuccess()) {
            const auto& valve_cfg = valve_coord_result.Value();
            if (valve_cfg.dispensing_interval_ms > 0) {
                params.dispenser_interval_ms = static_cast<uint32>(valve_cfg.dispensing_interval_ms);
            }
            if (valve_cfg.dispensing_duration_ms > 0) {
                params.dispenser_duration_ms = static_cast<uint32>(valve_cfg.dispensing_duration_ms);
            }
            if (valve_cfg.valve_response_ms >= 0) {
                params.valve_response_ms = static_cast<float32>(valve_cfg.valve_response_ms);
            }
            if (valve_cfg.visual_margin_ms >= 0) {
                params.safety_margin_ms = static_cast<float32>(valve_cfg.visual_margin_ms);
            }
            if (valve_cfg.min_interval_ms >= 0) {
                params.min_interval_ms = static_cast<float32>(valve_cfg.min_interval_ms);
            }
        }
    }

    if (request.trajectory_config.max_velocity > 0.0f) {
        params.dispensing_velocity = request.trajectory_config.max_velocity;
    }
    if (request.trajectory_config.max_acceleration > 0.0f) {
        params.acceleration = request.trajectory_config.max_acceleration;
    }
    if (request.trajectory_config.dispensing_interval > 0.0f) {
        params.trigger_spatial_interval_mm = request.trajectory_config.dispensing_interval;
    }

    if (safety_limit_velocity > 0.0f && params.dispensing_velocity > 0.0f) {
        params.dispensing_velocity = std::min(params.dispensing_velocity, safety_limit_velocity);
    }

    if (params.pulse_per_mm <= 0.0f) {
        params.pulse_per_mm = 200.0f;
    }
    if (params.dispenser_interval_ms == 0) {
        params.dispenser_interval_ms = 100;
    }
    if (params.dispenser_duration_ms == 0) {
        params.dispenser_duration_ms = 100;
    }
    if (params.trigger_spatial_interval_mm <= 0.0f) {
        params.trigger_spatial_interval_mm = 3.0f;
    }
    if (params.trigger_spatial_interval_mm <= 0.0f && params.dispensing_velocity > 0.0f) {
        params.trigger_spatial_interval_mm =
            params.dispensing_velocity * (static_cast<float32>(params.dispenser_interval_ms) / 1000.0f);
    }

    return params;
}

std::vector<Siligen::Shared::Types::Point2D> BuildTriggerPointsFromDistances(
    const std::vector<TrajectoryPoint>& trajectory,
    const std::vector<float32>& distances_mm) {
    std::vector<Siligen::Shared::Types::Point2D> points;
    if (trajectory.size() < 2 || distances_mm.empty()) {
        return points;
    }

    std::vector<float32> cumulative;
    cumulative.resize(trajectory.size(), 0.0f);
    for (size_t i = 1; i < trajectory.size(); ++i) {
        cumulative[i] = cumulative[i - 1] + trajectory[i - 1].position.DistanceTo(trajectory[i].position);
    }

    std::vector<float32> sorted = distances_mm;
    std::sort(sorted.begin(), sorted.end());

    size_t idx = 1;
    for (float32 target : sorted) {
        while (idx < cumulative.size() && cumulative[idx] + kEpsilon < target) {
            ++idx;
        }
        if (idx >= cumulative.size()) {
            break;
        }

        float32 seg_len = cumulative[idx] - cumulative[idx - 1];
        float32 ratio = (seg_len > kEpsilon) ? (target - cumulative[idx - 1]) / seg_len : 0.0f;
        auto start = Siligen::Shared::Types::Point2D(trajectory[idx - 1].position);
        auto end = Siligen::Shared::Types::Point2D(trajectory[idx].position);
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
                float32 dist = trajectory[i - 1].position.DistanceTo(trajectory[i].position);
                accumulated += dist;
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
    const float32 kNaN = std::numeric_limits<float32>::quiet_NaN();
    for (const auto& pt : trigger_points) {
        triggers.emplace_back(pt, kNaN);
    }

    return triggers;
}

std::vector<int32> CollectProcessTagsByDistance(const MotionTrajectory& trajectory,
                                                float32 spacing_mm,
                                                size_t max_points) {
    std::vector<int32> tags;
    if (trajectory.points.empty()) {
        return tags;
    }

    std::vector<float32> cumulative;
    cumulative.reserve(trajectory.points.size());
    cumulative.push_back(0.0f);
    for (size_t i = 1; i < trajectory.points.size(); ++i) {
        auto prev = Siligen::Shared::Types::Point2D(trajectory.points[i - 1].position);
        auto curr = Siligen::Shared::Types::Point2D(trajectory.points[i].position);
        cumulative.push_back(cumulative.back() + prev.DistanceTo(curr));
    }

    float32 total_length = cumulative.back();
    if (total_length <= kEpsilon) {
        tags.push_back(static_cast<int32>(trajectory.points.front().process_tag));
        return tags;
    }

    float32 step = (spacing_mm > kEpsilon) ? spacing_mm : 1.0f;
    if (max_points > 1) {
        float32 min_step = total_length / static_cast<float32>(max_points - 1);
        if (min_step > step) {
            step = min_step;
        }
    }
    if (step <= kEpsilon) {
        step = spacing_mm;
    }

    float32 next_dist = 0.0f;
    size_t seg_idx = 1;
    while (next_dist <= total_length + kEpsilon && seg_idx < trajectory.points.size()) {
        while (seg_idx < trajectory.points.size() && cumulative[seg_idx] + kEpsilon < next_dist) {
            ++seg_idx;
        }
        if (seg_idx >= trajectory.points.size()) {
            break;
        }
        tags.push_back(static_cast<int32>(trajectory.points[seg_idx - 1].process_tag));
        if (max_points > 0 && tags.size() >= max_points) {
            break;
        }
        next_dist += step;
    }

    if (!tags.empty()) {
        if (max_points > 0 && tags.size() >= max_points) {
            tags.back() = static_cast<int32>(trajectory.points.back().process_tag);
        } else if (tags.size() < trajectory.points.size()) {
            tags.push_back(static_cast<int32>(trajectory.points.back().process_tag));
        }
    }

    return tags;
}

CsvWriteResult WriteGluePointsCsv(const std::vector<GluePointSegment>& segments,
                                  const std::string& dxf_path) {
    CsvWriteResult result;
    const std::string csv_path = BuildGluePointsCsvPath(dxf_path);
    result.path = csv_path;

    try {
        std::filesystem::path path(csv_path);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
    } catch (...) {
        // ignore
    }

    std::ofstream file(csv_path, std::ios::binary);
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

    file.close();
    result.ok = true;
    result.point_count = global_index - 1;
    return result;
}

DumpWriteResult WriteTrajectoryPointsCsv(const std::vector<TrajectoryPoint>& points,
                                         const std::string& dxf_path,
                                         const std::string& tag,
                                         std::time_t timestamp) {
    DumpWriteResult result;
    const std::string csv_path = BuildTrajectoryDumpPath(dxf_path, tag, "csv", timestamp);
    result.path = csv_path;

    try {
        std::filesystem::path path(csv_path);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
    } catch (...) {
        // ignore
    }

    std::ofstream file(csv_path, std::ios::binary);
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
    const std::string csv_path = BuildTrajectoryDumpPath(dxf_path, tag, "csv", timestamp);
    result.path = csv_path;

    try {
        std::filesystem::path path_obj(csv_path);
        if (path_obj.has_parent_path()) {
            std::filesystem::create_directories(path_obj.parent_path());
        }
    } catch (...) {
        // ignore
    }

    std::ofstream file(csv_path, std::ios::binary);
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
    const std::string json_path = BuildTrajectoryDumpPath(dxf_path, tag, "json", timestamp);
    result.path = json_path;

    try {
        std::filesystem::path path(json_path);
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
    } catch (...) {
        // ignore
    }

    std::ofstream file(json_path, std::ios::binary);
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

DXFDispensingPlanRequest BuildPlanRequest(
    const DXFPlanningRequest& request,
    const PreviewRuntimeParams& runtime_params,
    const std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort>& config_port) {
    DXFDispensingPlanRequest plan_request;
    plan_request.dxf_filepath = request.dxf_filepath;
    plan_request.optimize_path = request.optimize_path;
    plan_request.start_x = request.start_x;
    plan_request.start_y = request.start_y;
    plan_request.approximate_splines = request.approximate_splines;
    plan_request.two_opt_iterations = request.two_opt_iterations;
    plan_request.spline_max_step_mm = request.spline_max_step_mm;
    plan_request.spline_max_error_mm = request.spline_max_error_mm;
    plan_request.continuity_tolerance_mm = request.continuity_tolerance_mm;
    plan_request.curve_chain_angle_deg = request.curve_chain_angle_deg;
    plan_request.curve_chain_max_segment_mm = request.curve_chain_max_segment_mm;
    plan_request.use_hardware_trigger = request.use_hardware_trigger;
    plan_request.dispensing_velocity = runtime_params.dispensing_velocity;
    plan_request.acceleration = runtime_params.acceleration;
    plan_request.dispenser_interval_ms = runtime_params.dispenser_interval_ms;
    plan_request.dispenser_duration_ms = runtime_params.dispenser_duration_ms;
    plan_request.trigger_spatial_interval_mm = runtime_params.trigger_spatial_interval_mm;
    plan_request.pulse_per_mm = runtime_params.pulse_per_mm;
    plan_request.valve_response_ms = runtime_params.valve_response_ms;
    plan_request.safety_margin_ms = runtime_params.safety_margin_ms;
    plan_request.min_interval_ms = runtime_params.min_interval_ms;
    plan_request.compensation_profile = runtime_params.compensation_profile;
    plan_request.sample_dt = runtime_params.sample_dt;
    plan_request.sample_ds = runtime_params.sample_ds;
    plan_request.arc_tolerance_mm = request.trajectory_config.arc_tolerance;
    plan_request.max_jerk = request.trajectory_config.max_jerk;
    plan_request.use_interpolation_planner = request.use_interpolation_planner;
    plan_request.interpolation_algorithm = request.interpolation_algorithm;

    if (!config_port) {
        return plan_request;
    }

    auto system_result = config_port->LoadConfiguration();
    if (system_result.IsSuccess()) {
        const auto& config = system_result.Value();
        plan_request.dxf_offset = Point2D(config.dxf.offset_x, config.dxf.offset_y);
    } else {
        SILIGEN_LOG_WARNING("读取系统配置失败: " + system_result.GetError().GetMessage());
    }

    auto dispensing_result = config_port->GetDispensingConfig();
    if (dispensing_result.IsSuccess()) {
        const auto& disp = dispensing_result.Value();
        plan_request.dispensing_strategy = disp.strategy;
        if (disp.subsegment_count > 0) {
            plan_request.subsegment_count = disp.subsegment_count;
        }
        plan_request.dispense_only_cruise = disp.dispense_only_cruise;
        plan_request.start_speed_factor = disp.start_speed_factor;
        plan_request.end_speed_factor = disp.end_speed_factor;
        plan_request.corner_speed_factor = disp.corner_speed_factor;
        plan_request.rapid_speed_factor = disp.rapid_speed_factor;
    }

    auto traj_result = config_port->GetDxfTrajectoryConfig();
    if (traj_result.IsSuccess()) {
        const auto& traj = traj_result.Value();
        plan_request.python_ruckig_python = traj.python;
        plan_request.python_ruckig_script = traj.script;
    }

    return plan_request;
}

ExecutionTrajectorySelection SelectExecutionTrajectory(const DXFDispensingPlan& plan, bool dump_preview) {
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

void DumpPreviewArtifacts(const DXFPlanningRequest& request,
                         const DXFDispensingPlan& plan,
                         const ExecutionTrajectorySelection& selection,
                         bool dump_preview,
                         std::time_t dump_timestamp) {
    if (!dump_preview) {
        return;
    }

    const auto process_csv = WriteProcessPathCsv(plan.process_path,
                                                 request.dxf_filepath,
                                                 "process_path",
                                                 dump_timestamp);
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

float32 ResolveTargetSpacing(const DXFDispensingPlan& plan, const PreviewRuntimeParams& runtime_params) {
    if (plan.trigger_interval_mm > kEpsilon) {
        return plan.trigger_interval_mm;
    }
    if (runtime_params.trigger_spatial_interval_mm > kEpsilon) {
        return runtime_params.trigger_spatial_interval_mm;
    }
    return 0.0f;
}

void ValidateGlueSpacing(const DXFPlanningRequest& request,
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

float32 EstimateExecutionTime(const DXFDispensingPlan& plan, const DXFDispensingPlanRequest& plan_request) {
    float32 estimated_time = plan.estimated_time_s;
    if (estimated_time <= kEpsilon) {
        estimated_time = plan.motion_trajectory.total_time;
    }
    if (estimated_time <= kEpsilon && plan.total_length_mm > kEpsilon && plan_request.dispensing_velocity > kEpsilon) {
        estimated_time = plan.total_length_mm / plan_request.dispensing_velocity;
    }
    return estimated_time;
}

DXFPlanningResponse BuildPlanningResponse(const DXFPlanningRequest& request,
                                         const DXFDispensingPlan& plan,
                                         const std::vector<TrajectoryPoint>* execution_trajectory,
                                         const std::vector<Siligen::Shared::Types::Point2D>& final_glue_points,
                                         const std::string& dxf_filename,
                                         float32 estimated_time) {
    std::vector<TrajectoryPoint> preview_points;
    if (execution_trajectory) {
        preview_points = *execution_trajectory;
    }

    DXFPlanningResponse response;
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

DXFWebPlanningUseCase::DXFWebPlanningUseCase(
    std::shared_ptr<DXFDispensingPlanner> planner,
    std::shared_ptr<Siligen::Domain::Configuration::Ports::IConfigurationPort> config_port,
    std::shared_ptr<Siligen::Application::Services::DXF::DxfPbPreparationService> pb_preparation_service)
    : planner_(std::move(planner)),
      config_port_(std::move(config_port)),
      pb_preparation_service_(pb_preparation_service
                                  ? std::move(pb_preparation_service)
                                  : std::make_shared<Siligen::Application::Services::DXF::DxfPbPreparationService>(
                                        config_port_)) {
    if (!planner_) {
        throw std::invalid_argument("DXFWebPlanningUseCase: planner cannot be null");
    }
}

Result<DXFPlanningResponse> DXFWebPlanningUseCase::Execute(const DXFPlanningRequest& request) {
    SILIGEN_LOG_INFO("Starting DXF planning...");
    const auto total_start = std::chrono::steady_clock::now();

    if (!request.Validate()) {
        return Result<DXFPlanningResponse>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, "Invalid request parameters", "DXFWebPlanningUseCase"));
    }

    auto file_validation = ValidateFileExists(request.dxf_filepath);
    if (file_validation.IsError()) {
        return Result<DXFPlanningResponse>::Failure(file_validation.GetError());
    }

    auto pb_result = pb_preparation_service_->EnsurePbReady(request.dxf_filepath);
    if (pb_result.IsError()) {
        return Result<DXFPlanningResponse>::Failure(pb_result.GetError());
    }

    const auto runtime_params = BuildPreviewRuntimeParams(request, config_port_);
    const auto plan_request = BuildPlanRequest(request, runtime_params, config_port_);

    const auto plan_start = std::chrono::steady_clock::now();
    auto plan_result = planner_->Plan(plan_request);
    if (plan_result.IsError()) {
        return Result<DXFPlanningResponse>::Failure(plan_result.GetError());
    }
    const auto plan_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - plan_start).count();

    const auto& plan = plan_result.Value();
    SILIGEN_LOG_INFO_FMT_HELPER("DXF规划完成: segments=%zu, time=%lldms",
                                plan.process_path.segments.size(),
                                static_cast<long long>(plan_elapsed_ms));

    const bool dump_preview = ShouldDumpPreviewTrajectory();
    const std::time_t dump_timestamp = dump_preview ? std::time(nullptr) : 0;
    auto selection = SelectExecutionTrajectory(plan, dump_preview);
    DumpPreviewArtifacts(request, plan, selection, dump_preview, dump_timestamp);

    PreviewTriggerConfig trigger_config;
    trigger_config.spatial_interval_mm = runtime_params.trigger_spatial_interval_mm;

    std::vector<VisualizationTriggerPoint> preview_triggers;
    if (selection.execution_trajectory) {
        preview_triggers = BuildPlannedTriggerPoints(*selection.execution_trajectory,
                                                     trigger_config,
                                                     plan.trigger_distances_mm);
    }
    if (preview_triggers.empty()) {
        return Result<DXFPlanningResponse>::Failure(
            Error(ErrorCode::CMP_TRIGGER_SETUP_FAILED,
                  "位置触发不可用，禁止回退为定时触发",
                  "DXFWebPlanningUseCase"));
    }

    const auto final_glue_points = CollectTriggerPositions(preview_triggers);
    const std::vector<GluePointSegment> glue_segments =
        final_glue_points.empty() ? std::vector<GluePointSegment>() : std::vector<GluePointSegment>{GluePointSegment{final_glue_points}};
    ValidateGlueSpacing(request, glue_segments, ResolveTargetSpacing(plan, runtime_params));
    WriteGluePointArtifacts(glue_segments, request.dxf_filepath);

    auto response = BuildPlanningResponse(request,
                                         plan,
                                         selection.execution_trajectory,
                                         final_glue_points,
                                         ExtractFilename(request.dxf_filepath),
                                         EstimateExecutionTime(plan, plan_request));

    SILIGEN_LOG_INFO_FMT_HELPER("DXF预览数据准备完成: points=%zu, triggers=%d",
                                response.trajectory_points.size(),
                                response.trigger_count);

    const auto total_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - total_start).count();
    SILIGEN_LOG_INFO_FMT_HELPER("DXF规划流程完成: total_time=%lldms", static_cast<long long>(total_elapsed_ms));

    return Result<DXFPlanningResponse>::Success(std::move(response));
}

Result<void> DXFWebPlanningUseCase::ValidateFileExists(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.good()) {
        return Result<void>::Failure(
            Error(ErrorCode::FILE_NOT_FOUND, "File not found: " + filepath, "DXFWebPlanningUseCase"));
    }
    return Result<void>::Success();
}

std::string DXFWebPlanningUseCase::ExtractFilename(const std::string& filepath) {
    try {
        std::filesystem::path path(filepath);
        return path.filename().string();
    } catch (...) {
        return filepath;
    }
}

}  // namespace Siligen::Application::UseCases::Dispensing::DXF

