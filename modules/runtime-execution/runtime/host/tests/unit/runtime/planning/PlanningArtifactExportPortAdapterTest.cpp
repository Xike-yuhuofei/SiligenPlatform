#include "runtime/planning/PlanningArtifactExportPortAdapter.h"

#include "dispense_packaging/contracts/PlanningArtifactExportRequest.h"
#include "process_path/contracts/ProcessPath.h"
#include "application/services/dispensing/PlanningArtifactExportPort.h"
#include "shared/types/Point.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

using IPlanningArtifactExportPort = Siligen::Application::Services::Dispensing::IPlanningArtifactExportPort;
using PlanningArtifactExportResult = Siligen::Application::Services::Dispensing::PlanningArtifactExportResult;
using PlanningArtifactExportRequest = Siligen::Domain::Dispensing::Contracts::PlanningArtifactExportRequest;
using ProcessPath = Siligen::ProcessPath::Contracts::ProcessPath;
using ProcessSegment = Siligen::ProcessPath::Contracts::ProcessSegment;
using ProcessTag = Siligen::ProcessPath::Contracts::ProcessTag;
using Segment = Siligen::ProcessPath::Contracts::Segment;
using SegmentType = Siligen::ProcessPath::Contracts::SegmentType;
using TrajectoryPoint = Siligen::TrajectoryPoint;

class ScopedPreviewTrajectoryEnv final {
   public:
    explicit ScopedPreviewTrajectoryEnv(const char* value) {
        const char* current = std::getenv("SILIGEN_DUMP_PREVIEW_TRAJECTORY");
        if (current) {
            had_original_ = true;
            original_value_ = current;
        }
        Set(value);
    }

    ~ScopedPreviewTrajectoryEnv() {
        if (had_original_) {
            Set(original_value_.c_str());
        } else {
            Set(nullptr);
        }
    }

   private:
    static void Set(const char* value) {
#ifdef _WIN32
        _putenv_s("SILIGEN_DUMP_PREVIEW_TRAJECTORY", value ? value : "");
#else
        if (value) {
            setenv("SILIGEN_DUMP_PREVIEW_TRAJECTORY", value, 1);
        } else {
            unsetenv("SILIGEN_DUMP_PREVIEW_TRAJECTORY");
        }
#endif
    }

    bool had_original_ = false;
    std::string original_value_;
};

std::filesystem::path MakeTempDir() {
    const auto unique = std::to_string(std::rand()) + "_" + std::to_string(std::time(nullptr));
    auto dir = std::filesystem::temp_directory_path() / ("planning_export_adapter_" + unique);
    std::filesystem::create_directories(dir);
    return dir;
}

ProcessPath BuildProcessPath() {
    ProcessPath path;
    ProcessSegment segment;
    segment.dispense_on = true;
    segment.tag = ProcessTag::Normal;
    segment.geometry.type = SegmentType::Line;
    segment.geometry.line.start = Siligen::Shared::Types::Point2D(1.0f, 2.0f);
    segment.geometry.line.end = Siligen::Shared::Types::Point2D(11.0f, 12.0f);
    segment.geometry.length = 14.0f;
    path.segments.push_back(segment);
    return path;
}

std::vector<TrajectoryPoint> BuildTrajectoryPoints() {
    TrajectoryPoint start(1.0f, 2.0f, 3.0f);
    start.timestamp = 0.1f;
    start.enable_position_trigger = true;
    TrajectoryPoint end(11.0f, 12.0f, 4.0f);
    end.timestamp = 0.2f;
    return {start, end};
}

PlanningArtifactExportRequest BuildRequest(
    const std::filesystem::path& dir,
    bool include_glue_points = true) {
    PlanningArtifactExportRequest request;
    request.source_path = (dir / "preview.pb").string();
    request.dxf_filename = "preview.dxf";
    request.process_path = BuildProcessPath();
    request.execution_trajectory_points = BuildTrajectoryPoints();
    request.interpolation_trajectory_points = BuildTrajectoryPoints();
    request.motion_trajectory_points = BuildTrajectoryPoints();
    if (include_glue_points) {
        request.glue_points.push_back(Siligen::Shared::Types::Point2D(5.0f, 6.0f));
    }
    return request;
}

std::vector<std::filesystem::path> CollectExportedFiles(
    const std::filesystem::path& dir,
    const std::string& pattern) {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().filename().string().find(pattern) != std::string::npos) {
            files.push_back(entry.path());
        }
    }
    return files;
}

TEST(PlanningArtifactExportPortAdapterTest, FactoryReturnsConsumerPortInstance) {
    auto port = Siligen::RuntimeExecution::Host::Planning::CreatePlanningArtifactExportPort();

    ASSERT_NE(port, nullptr);
}

TEST(PlanningArtifactExportPortAdapterTest, ExportReturnsNotRequestedWhenPreviewDumpEnvDisabled) {
    ScopedPreviewTrajectoryEnv env(nullptr);
    auto port = Siligen::RuntimeExecution::Host::Planning::CreatePlanningArtifactExportPort();
    auto temp_dir = MakeTempDir();

    auto result = port->Export(BuildRequest(temp_dir));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_FALSE(result.Value().export_requested);
    EXPECT_TRUE(result.Value().exported_paths.empty());

    std::filesystem::remove_all(temp_dir);
}

TEST(PlanningArtifactExportPortAdapterTest, ExportCreatesProcessPathAndTrajectoryArtifactsWhenEnabled) {
    ScopedPreviewTrajectoryEnv env("1");
    auto port = Siligen::RuntimeExecution::Host::Planning::CreatePlanningArtifactExportPort();
    auto temp_dir = MakeTempDir();

    auto result = port->Export(BuildRequest(temp_dir));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    ASSERT_TRUE(result.Value().export_requested);
    EXPECT_TRUE(result.Value().success);
    EXPECT_GE(result.Value().exported_paths.size(), 4U);
    EXPECT_FALSE(CollectExportedFiles(temp_dir, "process_path").empty());
    EXPECT_FALSE(CollectExportedFiles(temp_dir, "execution_trajectory").empty());
    EXPECT_FALSE(CollectExportedFiles(temp_dir, "interpolation_trajectory").empty());
    EXPECT_FALSE(CollectExportedFiles(temp_dir, "motion_trajectory").empty());
    EXPECT_FALSE(CollectExportedFiles(temp_dir, "glue_points").empty());

    std::filesystem::remove_all(temp_dir);
}

TEST(PlanningArtifactExportPortAdapterTest, ExportSkipsGlueCsvWhenGluePointsAbsent) {
    ScopedPreviewTrajectoryEnv env("true");
    auto port = Siligen::RuntimeExecution::Host::Planning::CreatePlanningArtifactExportPort();
    auto temp_dir = MakeTempDir();

    auto result = port->Export(BuildRequest(temp_dir, false));

    ASSERT_TRUE(result.IsSuccess()) << result.GetError().GetMessage();
    EXPECT_TRUE(result.Value().export_requested);
    EXPECT_TRUE(CollectExportedFiles(temp_dir, "glue_points").empty());

    std::filesystem::remove_all(temp_dir);
}

}  // namespace
