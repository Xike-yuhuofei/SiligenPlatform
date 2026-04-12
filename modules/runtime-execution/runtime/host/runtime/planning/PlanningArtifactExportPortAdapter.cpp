#include "PlanningArtifactExportPortAdapter.h"

#include "shared/interfaces/ILoggingService.h"
#include "shared/types/Error.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>

namespace Siligen::RuntimeExecution::Host::Planning {

using Siligen::Application::Services::Dispensing::PlanningArtifactExportResult;
using Siligen::Domain::Dispensing::Contracts::PlanningArtifactExportRequest;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

namespace {

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

std::string BuildArtifactPath(const std::string& source_path,
                              const std::string& stem_suffix,
                              const std::string& extension,
                              std::time_t timestamp,
                              bool timestamped) {
    std::filesystem::path source(source_path);
    std::string stem = source.stem().string();
    if (stem.empty()) {
        stem = "trajectory";
    }

    std::string filename = stem + "_" + stem_suffix;
    if (timestamped) {
        filename += "_" + std::to_string(timestamp);
    }
    filename += "." + extension;

    if (source.has_parent_path()) {
        return (source.parent_path() / filename).string();
    }
    return filename;
}

Result<void> EnsureParentDirectory(const std::string& path_value) {
    std::filesystem::path path(path_value);
    if (!path.has_parent_path()) {
        return Result<void>::Success();
    }

    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        return Result<void>::Failure(Error(
            ErrorCode::FILE_IO_ERROR,
            "Failed to create directory for planning artifact: " + ec.message(),
            "PlanningArtifactExportPortAdapter"));
    }
    return Result<void>::Success();
}

Result<void> WriteGluePointsCsv(const PlanningArtifactExportRequest& request,
                                const std::string& path_value) {
    auto ensure_result = EnsureParentDirectory(path_value);
    if (ensure_result.IsError()) {
        return ensure_result;
    }

    std::ofstream file(path_value, std::ios::binary);
    if (!file) {
        return Result<void>::Failure(Error(
            ErrorCode::FILE_IO_ERROR,
            "Failed to open glue points CSV: " + path_value,
            "PlanningArtifactExportPortAdapter"));
    }

    file.setf(std::ios::fixed);
    file << std::setprecision(6);
    file << "index,segment_index,segment_type,trigger_index,x_mm,y_mm\n";
    for (size_t index = 0; index < request.glue_points.size(); ++index) {
        const auto& point = request.glue_points[index];
        file << (index + 1) << ",0,TRAJ," << (index + 1) << ','
             << point.x << ',' << point.y << '\n';
    }
    return Result<void>::Success();
}

Siligen::Shared::Types::Point2D ArcPoint(const Siligen::ProcessPath::Contracts::ArcPrimitive& arc, float32 angle_deg) {
    constexpr float32 kPi = 3.14159265359f;
    constexpr float32 kDegToRad = kPi / 180.0f;
    const float32 angle_rad = angle_deg * kDegToRad;
    return Siligen::Shared::Types::Point2D(
        arc.center.x + arc.radius * std::cos(angle_rad),
        arc.center.y + arc.radius * std::sin(angle_rad));
}

Siligen::Shared::Types::Point2D SegmentStart(const Siligen::ProcessPath::Contracts::Segment& segment) {
    using SegmentType = Siligen::ProcessPath::Contracts::SegmentType;
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

Siligen::Shared::Types::Point2D SegmentEnd(const Siligen::ProcessPath::Contracts::Segment& segment) {
    using SegmentType = Siligen::ProcessPath::Contracts::SegmentType;
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

const char* SegmentTypeLabel(Siligen::ProcessPath::Contracts::SegmentType type) {
    using SegmentType = Siligen::ProcessPath::Contracts::SegmentType;
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

const char* ProcessTagLabel(Siligen::ProcessPath::Contracts::ProcessTag tag) {
    using ProcessTag = Siligen::ProcessPath::Contracts::ProcessTag;
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

Result<void> WriteProcessPathCsv(const PlanningArtifactExportRequest& request,
                                 const std::string& path_value) {
    auto ensure_result = EnsureParentDirectory(path_value);
    if (ensure_result.IsError()) {
        return ensure_result;
    }

    std::ofstream file(path_value, std::ios::binary);
    if (!file) {
        return Result<void>::Failure(Error(
            ErrorCode::FILE_IO_ERROR,
            "Failed to open process path CSV: " + path_value,
            "PlanningArtifactExportPortAdapter"));
    }

    file.setf(std::ios::fixed);
    file << std::setprecision(6);
    file << "index,dispense_on,tag,type,start_x,start_y,end_x,end_y,length_mm\n";
    size_t index = 0;
    for (const auto& seg : request.process_path.segments) {
        const auto start = SegmentStart(seg.geometry);
        const auto end = SegmentEnd(seg.geometry);
        file << index << ',' << (seg.dispense_on ? 1 : 0) << ','
             << ProcessTagLabel(seg.tag) << ','
             << SegmentTypeLabel(seg.geometry.type) << ','
             << start.x << ',' << start.y << ','
             << end.x << ',' << end.y << ','
             << seg.geometry.length << '\n';
        ++index;
    }
    return Result<void>::Success();
}

Result<void> WriteTrajectoryCsv(const std::vector<TrajectoryPoint>& points,
                                const std::string& path_value) {
    auto ensure_result = EnsureParentDirectory(path_value);
    if (ensure_result.IsError()) {
        return ensure_result;
    }

    std::ofstream file(path_value, std::ios::binary);
    if (!file) {
        return Result<void>::Failure(Error(
            ErrorCode::FILE_IO_ERROR,
            "Failed to open trajectory CSV: " + path_value,
            "PlanningArtifactExportPortAdapter"));
    }

    file.setf(std::ios::fixed);
    file << std::setprecision(6);
    file << "index,x_mm,y_mm,velocity_mm_s,time_s,trigger\n";
    for (size_t index = 0; index < points.size(); ++index) {
        const auto& pt = points[index];
        file << (index + 1) << ',' << pt.position.x << ',' << pt.position.y << ','
             << pt.velocity << ',' << pt.timestamp << ','
             << (pt.enable_position_trigger ? 1 : 0) << '\n';
    }
    return Result<void>::Success();
}

Result<void> WriteTrajectoryJson(const std::vector<TrajectoryPoint>& points,
                                 const std::string& tag,
                                 const std::string& path_value) {
    auto ensure_result = EnsureParentDirectory(path_value);
    if (ensure_result.IsError()) {
        return ensure_result;
    }

    std::ofstream file(path_value, std::ios::binary);
    if (!file) {
        return Result<void>::Failure(Error(
            ErrorCode::FILE_IO_ERROR,
            "Failed to open trajectory JSON: " + path_value,
            "PlanningArtifactExportPortAdapter"));
    }

    file.setf(std::ios::fixed);
    file << std::setprecision(6);
    file << "{\n";
    file << "  \"source\": \"" << tag << "\",\n";
    file << "  \"point_count\": " << points.size() << ",\n";
    file << "  \"points\": [\n";
    for (size_t index = 0; index < points.size(); ++index) {
        const auto& pt = points[index];
        file << "    [" << pt.position.x << "," << pt.position.y << ","
             << pt.velocity << "," << pt.timestamp << ","
             << (pt.enable_position_trigger ? 1 : 0) << "]";
        if (index + 1 < points.size()) {
            file << ",";
        }
        file << "\n";
    }
    file << "  ]\n";
    file << "}\n";
    return Result<void>::Success();
}

Result<void> ExportTrajectoryArtifacts(const PlanningArtifactExportRequest& request,
                                       const std::vector<TrajectoryPoint>& points,
                                       const std::string& tag,
                                       std::time_t timestamp,
                                       std::vector<std::string>* exported_paths) {
    if (points.empty()) {
        return Result<void>::Success();
    }

    const auto csv_path = BuildArtifactPath(request.source_path, tag + "_trajectory", "csv", timestamp, true);
    auto csv_result = WriteTrajectoryCsv(points, csv_path);
    if (csv_result.IsError()) {
        return csv_result;
    }
    exported_paths->push_back(csv_path);

    const auto json_path = BuildArtifactPath(request.source_path, tag + "_trajectory", "json", timestamp, true);
    auto json_result = WriteTrajectoryJson(points, tag, json_path);
    if (json_result.IsError()) {
        return json_result;
    }
    exported_paths->push_back(json_path);
    return Result<void>::Success();
}

}  // namespace

Result<PlanningArtifactExportResult> PlanningArtifactExportPortAdapter::Export(
    const PlanningArtifactExportRequest& request) {
    PlanningArtifactExportResult result;
    result.export_requested = ShouldDumpPreviewTrajectory();
    if (!result.export_requested) {
        return Result<PlanningArtifactExportResult>::Success(result);
    }

    const auto timestamp = std::time(nullptr);

    const auto process_path_csv = BuildArtifactPath(request.source_path, "process_path", "csv", timestamp, true);
    auto process_result = WriteProcessPathCsv(request, process_path_csv);
    if (process_result.IsError()) {
        return Result<PlanningArtifactExportResult>::Failure(process_result.GetError());
    }
    result.exported_paths.push_back(process_path_csv);

    auto execution_result = ExportTrajectoryArtifacts(
        request,
        request.execution_trajectory_points,
        "execution",
        timestamp,
        &result.exported_paths);
    if (execution_result.IsError()) {
        return Result<PlanningArtifactExportResult>::Failure(execution_result.GetError());
    }

    auto interpolation_result = ExportTrajectoryArtifacts(
        request,
        request.interpolation_trajectory_points,
        "interpolation",
        timestamp,
        &result.exported_paths);
    if (interpolation_result.IsError()) {
        return Result<PlanningArtifactExportResult>::Failure(interpolation_result.GetError());
    }

    auto motion_result = ExportTrajectoryArtifacts(
        request,
        request.motion_trajectory_points,
        "motion",
        timestamp,
        &result.exported_paths);
    if (motion_result.IsError()) {
        return Result<PlanningArtifactExportResult>::Failure(motion_result.GetError());
    }

    if (!request.glue_points.empty()) {
        const auto glue_csv = BuildArtifactPath(request.source_path, "glue_points", "csv", 0, false);
        auto glue_result = WriteGluePointsCsv(request, glue_csv);
        if (glue_result.IsError()) {
            return Result<PlanningArtifactExportResult>::Failure(glue_result.GetError());
        }
        result.exported_paths.push_back(glue_csv);
    }

    result.success = true;
    result.message = "planning artifacts exported";
    SILIGEN_LOG_INFO("Planning artifacts exported: count=" + std::to_string(result.exported_paths.size()));
    return Result<PlanningArtifactExportResult>::Success(std::move(result));
}

std::shared_ptr<Siligen::Application::Services::Dispensing::IPlanningArtifactExportPort> CreatePlanningArtifactExportPort() {
    return std::make_shared<PlanningArtifactExportPortAdapter>();
}

}  // namespace Siligen::RuntimeExecution::Host::Planning
