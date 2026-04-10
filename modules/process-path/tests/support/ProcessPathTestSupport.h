#pragma once

#include "application/services/process_path/ProcessPathFacade.h"
#include "dxf_geometry/adapters/planning/dxf/PbPathSourceAdapter.h"
#include "motion_planning/contracts/TimePlanningConfig.h"
#include "process_path/contracts/Path.h"
#include "process_path/contracts/PathPrimitiveMeta.h"
#include "process_path/contracts/ProcessPath.h"
#include "process_path/contracts/Primitive.h"

#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace Siligen::ProcessPath::Tests::Support {

using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
using Siligen::Application::Services::ProcessPath::ProcessPathBuildResult;
using Siligen::Infrastructure::Adapters::Parsing::PbPathSourceAdapter;
using Siligen::MotionPlanning::Contracts::TimePlanningConfig;
using Siligen::ProcessPath::Contracts::PathGenerationRequest;
using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;
using Siligen::ProcessPath::Contracts::Primitive;
using Siligen::ProcessPath::Contracts::ProcessPath;
using Siligen::ProcessPath::Contracts::ProcessSegment;
using Siligen::ProcessPath::Contracts::ProcessTag;
using Siligen::ProcessPath::Contracts::Segment;
using Siligen::ProcessPath::Contracts::SegmentType;
using Siligen::Shared::Types::Point2D;

namespace fs = std::filesystem;

inline fs::path WorkspaceRoot() {
    return fs::path(__FILE__).parent_path().parent_path().parent_path().parent_path().parent_path();
}

inline fs::path EngineeringFixtureCasePath(const std::string& case_name, const std::string& file_name) {
    return WorkspaceRoot() / "shared" / "contracts" / "engineering" / "fixtures" / "cases" / case_name / file_name;
}

inline ProcessPathBuildRequest LoadFixtureRequest(const fs::path& fixture_path) {
    PbPathSourceAdapter adapter;
    const auto load_result = adapter.LoadFromFile(fixture_path.string());
    if (!load_result.IsSuccess()) {
        throw std::runtime_error(load_result.GetError().ToString());
    }

    ProcessPathBuildRequest request;
    request.primitives = load_result.Value().primitives;
    request.metadata.reserve(load_result.Value().metadata.size());
    for (const auto& item : load_result.Value().metadata) {
        PathPrimitiveMeta meta;
        meta.entity_id = item.entity_id;
        meta.entity_type = item.entity_type;
        meta.entity_segment_index = item.entity_segment_index;
        meta.entity_closed = item.entity_closed;
        request.metadata.push_back(meta);
    }
    return request;
}

inline ProcessPathBuildRequest MakeLeadInLeadOutRequest(const bool enable_priming = false) {
    ProcessPathBuildRequest request;
    request.primitives.push_back(Primitive::MakeLine(Point2D(0.0f, 0.0f), Point2D(10.0f, 0.0f)));
    request.normalization.continuity_tolerance = 0.1f;
    request.process.default_flow = 2.5f;
    request.process.approach_dist = 1.0f;
    request.process.lead_off_dist = 1.0f;
    request.process.priming.enable = enable_priming;
    request.process.priming.flow_rate = 1.25f;
    request.shaping.corner_smoothing_radius = 0.5f;
    request.shaping.position_tolerance = 0.1f;
    return request;
}

inline TimePlanningConfig MakeMotionPlanningConfig() {
    TimePlanningConfig config;
    config.vmax = 20.0f;
    config.amax = 100.0f;
    config.sample_dt = 0.01f;
    return config;
}

inline const char* SegmentTypeName(const SegmentType type) {
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

inline const char* ProcessTagName(const ProcessTag tag) {
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

inline void SerializeSegment(std::ostringstream& out, const std::string& prefix, const Segment& segment) {
    out << prefix << ".type=" << SegmentTypeName(segment.type) << "\n";
    out << prefix << ".length=" << segment.length << "\n";
    if (segment.type == SegmentType::Line) {
        out << prefix << ".start=(" << segment.line.start.x << "," << segment.line.start.y << ")\n";
        out << prefix << ".end=(" << segment.line.end.x << "," << segment.line.end.y << ")\n";
    }
}

inline void SerializeProcessSegment(std::ostringstream& out,
                                    const std::string& prefix,
                                    const ProcessSegment& segment) {
    out << prefix << ".tag=" << ProcessTagName(segment.tag) << "\n";
    out << prefix << ".dispense_on=" << std::boolalpha << segment.dispense_on << "\n";
    out << prefix << ".flow_rate=" << segment.flow_rate << "\n";
    SerializeSegment(out, prefix + ".geometry", segment.geometry);
}

inline std::string SerializeBuildResultSummary(const ProcessPathBuildResult& result) {
    std::ostringstream out;
    out << std::boolalpha << std::fixed << std::setprecision(3);
    out << "status=" << static_cast<int>(result.status) << "\n";
    out << "failure_stage=" << static_cast<int>(result.failed_stage) << "\n";
    out << "normalized.segment_count=" << result.normalized.path.segments.size() << "\n";
    out << "normalized.closed=" << result.normalized.path.closed << "\n";
    out << "normalized.discontinuity_count=" << result.normalized.report.discontinuity_count << "\n";
    for (size_t i = 0; i < result.normalized.path.segments.size(); ++i) {
        SerializeSegment(out, "normalized.segment[" + std::to_string(i) + "]", result.normalized.path.segments[i]);
    }

    out << "process.segment_count=" << result.process_path.segments.size() << "\n";
    for (size_t i = 0; i < result.process_path.segments.size(); ++i) {
        SerializeProcessSegment(out, "process.segment[" + std::to_string(i) + "]", result.process_path.segments[i]);
    }

    out << "shaped.segment_count=" << result.shaped_path.segments.size() << "\n";
    for (size_t i = 0; i < result.shaped_path.segments.size(); ++i) {
        SerializeProcessSegment(out, "shaped.segment[" + std::to_string(i) + "]", result.shaped_path.segments[i]);
    }
    return out.str();
}

}  // namespace Siligen::ProcessPath::Tests::Support
