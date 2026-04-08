#include "application/services/motion_planning/MotionPlanningFacade.h"
#include "application/services/process_path/ProcessPathFacade.h"
#include "dxf_geometry/adapters/planning/dxf/PbPathSourceAdapter.h"

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>

TEST(MotionPlanningFacadeTest, PlansTrajectoryFromProcessPath) {
    using Siligen::Application::Services::MotionPlanning::MotionPlanningFacade;
    using Siligen::MotionPlanning::Contracts::TimePlanningConfig;
    using Siligen::ProcessPath::Contracts::ProcessPath;
    using Siligen::ProcessPath::Contracts::ProcessSegment;
    using Siligen::ProcessPath::Contracts::ProcessTag;
    using Siligen::ProcessPath::Contracts::SegmentType;
    using Siligen::Shared::Types::Point2D;

    ProcessPath path;
    ProcessSegment seg;
    seg.dispense_on = true;
    seg.flow_rate = 1.0f;
    seg.tag = ProcessTag::Normal;
    seg.geometry.type = SegmentType::Line;
    seg.geometry.line.start = Point2D(0.0f, 0.0f);
    seg.geometry.line.end = Point2D(10.0f, 0.0f);
    seg.geometry.length = 10.0f;
    path.segments.push_back(seg);

    TimePlanningConfig config;
    config.vmax = 50.0f;
    config.amax = 200.0f;
    config.sample_dt = 0.01f;

    MotionPlanningFacade facade;
    const auto trajectory = facade.Plan(path, config);

    ASSERT_FALSE(trajectory.points.empty());
    EXPECT_GT(trajectory.total_time, 0.0f);
}

namespace {

namespace fs = std::filesystem;

std::vector<std::array<size_t, 3>> CollectImmediateDirectionReversals(
    const Siligen::MotionPlanning::Contracts::MotionTrajectory& trajectory) {
    std::vector<std::array<size_t, 3>> reversals;
    for (size_t index = 2; index < trajectory.points.size(); ++index) {
        const auto& a = trajectory.points[index - 2].position;
        const auto& b = trajectory.points[index - 1].position;
        const auto& c = trajectory.points[index].position;
        const Siligen::Shared::Types::Point2D ab(b.x - a.x, b.y - a.y);
        const Siligen::Shared::Types::Point2D bc(c.x - b.x, c.y - b.y);
        const auto ab_len = ab.Length();
        const auto bc_len = bc.Length();
        if (ab_len <= 1e-4f || bc_len <= 1e-4f) {
            continue;
        }
        const auto normalized_cross = std::abs(ab.Cross(bc)) / std::max(1e-4f, ab_len * bc_len);
        const auto normalized_dot = ab.Dot(bc) / (ab_len * bc_len);
        if (normalized_cross <= 1e-3f && normalized_dot < -0.95f) {
            reversals.push_back({index - 2, index - 1, index});
        }
    }
    return reversals;
}

void WriteTrajectoryCsv(
    const fs::path& output_path,
    const Siligen::MotionPlanning::Contracts::MotionTrajectory& trajectory) {
    std::ofstream out(output_path);
    out << "point_index,t,x,y,z,vx,vy,vz,dispense_on,process_tag\n";
    for (size_t index = 0; index < trajectory.points.size(); ++index) {
        const auto& point = trajectory.points[index];
        out << index << ','
            << point.t << ','
            << point.position.x << ','
            << point.position.y << ','
            << point.position.z << ','
            << point.velocity.x << ','
            << point.velocity.y << ','
            << point.velocity.z << ','
            << (point.dispense_on ? 1 : 0) << ','
            << static_cast<int>(point.process_tag) << '\n';
    }
}

}  // namespace

TEST(MotionPlanningFacadeTest, DemoPbRealFileDiagnosticsExportsMotionTrajectoryFacts) {
    const fs::path pb_path = "D:\\Projects\\wt-task157\\tmp\\Demo.pb";
    if (!fs::exists(pb_path)) {
        GTEST_SKIP() << "Demo.pb 不存在: " << pb_path.string();
    }

    Siligen::Infrastructure::Adapters::Parsing::PbPathSourceAdapter adapter;
    const auto load_result = adapter.LoadFromFile(pb_path.string());
    ASSERT_TRUE(load_result.IsSuccess()) << load_result.GetError().ToString();

    using Siligen::Application::Services::MotionPlanning::MotionPlanningFacade;
    using Siligen::Application::Services::ProcessPath::ProcessPathBuildRequest;
    using Siligen::Application::Services::ProcessPath::ProcessPathFacade;
    using Siligen::MotionPlanning::Contracts::TimePlanningConfig;
    using Siligen::ProcessPath::Contracts::PathGenerationStatus;
    using Siligen::ProcessPath::Contracts::PathPrimitiveMeta;

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
    request.normalization.continuity_tolerance = 0.1f;
    request.topology_repair.enable = true;
    request.topology_repair.start_position = Siligen::Shared::Types::Point2D(0.0f, 0.0f);
    request.topology_repair.two_opt_iterations = 0;

    ProcessPathFacade process_path_facade;
    const auto path_result = process_path_facade.Build(request);
    ASSERT_EQ(path_result.status, PathGenerationStatus::Success) << path_result.error_message;

    TimePlanningConfig config;
    config.vmax = 50.0f;
    config.amax = 200.0f;
    config.sample_dt = 0.01f;

    MotionPlanningFacade facade;
    const auto trajectory = facade.Plan(path_result.shaped_path, config);

    ASSERT_FALSE(trajectory.points.empty());

    const auto reversals = CollectImmediateDirectionReversals(trajectory);

    const fs::path output_dir = "D:\\Projects\\wt-task157\\tmp\\demo-diagnostics";
    fs::create_directories(output_dir);
    const auto csv_path = output_dir / "demo_motion_trajectory.csv";
    WriteTrajectoryCsv(csv_path, trajectory);

    for (const auto& item : reversals) {
        const auto& a = trajectory.points[item[0]].position;
        const auto& b = trajectory.points[item[1]].position;
        const auto& c = trajectory.points[item[2]].position;
        std::cout
            << "Demo motion_reversal_triplet"
            << " idx=" << item[0] << '/' << item[1] << '/' << item[2]
            << " a=(" << a.x << ',' << a.y << ')'
            << " b=(" << b.x << ',' << b.y << ')'
            << " c=(" << c.x << ',' << c.y << ')'
            << std::endl;
    }

    std::cout
        << "Demo motion_trajectory"
        << " points=" << trajectory.points.size()
        << " reversal_triplets=" << reversals.size()
        << " total_time=" << trajectory.total_time
        << " total_length=" << trajectory.total_length
        << " csv=" << csv_path.string()
        << std::endl;
}
