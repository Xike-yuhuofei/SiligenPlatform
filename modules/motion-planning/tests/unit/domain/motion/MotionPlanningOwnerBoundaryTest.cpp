#include "domain/motion/BezierCalculator.h"
#include "domain/motion/BSplineCalculator.h"
#include "domain/motion/CMPCoordinatedInterpolator.h"
#include "domain/motion/CMPCompensation.h"
#include "domain/motion/CMPValidator.h"
#include "domain/motion/CircleCalculator.h"
#include "domain/motion/domain-services/interpolation/InterpolationProgramPlanner.h"
#include "domain/motion/domain-services/TimeTrajectoryPlanner.h"
#include "domain/motion/domain-services/TrajectoryPlanner.h"
#include "motion_planning/contracts/MotionTrajectory.h"
#include "motion_planning/contracts/TimePlanningConfig.h"
#include "process_path/contracts/ProcessPath.h"
#include "runtime_execution/contracts/motion/IInterpolationPort.h"

#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace {

namespace fs = std::filesystem;

fs::path RepoRoot() {
    fs::path current = fs::path(__FILE__).lexically_normal().parent_path();
    for (int i = 0; i < 6; ++i) {
        current = current.parent_path();
    }
    return current;
}

std::string ReadTextFile(const fs::path& path) {
    std::ifstream input(path, std::ios::in | std::ios::binary);
    EXPECT_TRUE(input.is_open()) << "failed to open " << path.string();
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

TEST(MotionPlanningOwnerBoundaryTest, CanonicalPlanningHeadersUseProcessPathContractsSurface) {
    using Interpolator = Siligen::Domain::Motion::CMPCoordinatedInterpolator;
    using InterpolationProgramPlanner = Siligen::Domain::Motion::DomainServices::InterpolationProgramPlanner;
    using TimeTrajectoryPlanner = Siligen::Domain::Motion::DomainServices::TimeTrajectoryPlanner;
    using MotionTrajectory = Siligen::Domain::Motion::ValueObjects::MotionTrajectory;
    using TimePlanningConfig = Siligen::Domain::Motion::ValueObjects::TimePlanningConfig;
    using ContractsProcessPath = Siligen::ProcessPath::Contracts::ProcessPath;
    using InterpolationData = Siligen::Domain::Motion::Ports::InterpolationData;
    using InterpolationResult = Siligen::Shared::Types::Result<std::vector<InterpolationData>>;
    using Float32 = Siligen::Shared::Types::float32;

    using TimeTrajectoryPlanSignature =
        MotionTrajectory (TimeTrajectoryPlanner::*)(const ContractsProcessPath&, const TimePlanningConfig&) const;
    using ProcessPathCmpSignature =
        std::vector<Siligen::TrajectoryPoint> (Interpolator::*)(const ContractsProcessPath&,
                                                                const std::vector<Siligen::DispensingTriggerPoint>&,
                                                                const Siligen::CMPConfiguration&,
                                                                const Siligen::InterpolationConfig&);
    using InterpolationProgramBuildSignature =
        InterpolationResult (InterpolationProgramPlanner::*)(const ContractsProcessPath&,
                                                             const MotionTrajectory&,
                                                             Float32) const noexcept;

    static_assert(std::is_same_v<
                  decltype(static_cast<TimeTrajectoryPlanSignature>(&TimeTrajectoryPlanner::Plan)),
                  TimeTrajectoryPlanSignature>);
    static_assert(std::is_same_v<
                  decltype(static_cast<ProcessPathCmpSignature>(&Interpolator::PositionTriggeredDispensing)),
                  ProcessPathCmpSignature>);
    static_assert(std::is_same_v<
                  decltype(static_cast<InterpolationProgramBuildSignature>(&InterpolationProgramPlanner::BuildProgram)),
                  InterpolationProgramBuildSignature>);
    static_assert(std::is_class_v<Siligen::Domain::Motion::TrajectoryPlanner>);
    static_assert(std::is_class_v<Siligen::Domain::Motion::BezierCalculator>);
    static_assert(std::is_class_v<Siligen::Domain::Motion::BSplineCalculator>);
    static_assert(std::is_class_v<Siligen::Domain::Motion::CircleCalculator>);
    static_assert(std::is_class_v<Siligen::Domain::Motion::CMPCompensation>);
    static_assert(std::is_class_v<Siligen::Domain::Motion::CMPValidator>);
}

TEST(MotionPlanningOwnerBoundaryTest, WorkflowDomainRequiresCanonicalMotionOwnerTarget) {
    const fs::path repo_root = RepoRoot();
    const std::string workflow_cmake =
        ReadTextFile(repo_root / "modules/workflow/domain/domain/CMakeLists.txt");

    EXPECT_EQ(workflow_cmake.find("add_library(siligen_motion STATIC"), std::string::npos);
    EXPECT_NE(workflow_cmake.find("requires canonical siligen_motion target"), std::string::npos);
}

TEST(MotionPlanningOwnerBoundaryTest, WorkflowPlanningHeadersAreThinCompatibilityShims) {
    const fs::path repo_root = RepoRoot();
    const std::array<std::pair<fs::path, std::string>, 30> expectations = {{
        {repo_root / "modules/workflow/domain/domain/motion/CMPCoordinatedInterpolator.h",
         "#include \"../../../../motion-planning/domain/motion/CMPCoordinatedInterpolator.h\""},
        {repo_root / "modules/workflow/domain/domain/motion/BezierCalculator.h",
         "#include \"../../../../motion-planning/domain/motion/BezierCalculator.h\""},
        {repo_root / "modules/workflow/domain/domain/motion/BSplineCalculator.h",
         "#include \"../../../../motion-planning/domain/motion/BSplineCalculator.h\""},
        {repo_root / "modules/workflow/domain/domain/motion/CircleCalculator.h",
         "#include \"../../../../motion-planning/domain/motion/CircleCalculator.h\""},
        {repo_root / "modules/workflow/domain/domain/motion/CMPCompensation.h",
         "#include \"../../../../motion-planning/domain/motion/CMPCompensation.h\""},
        {repo_root / "modules/workflow/domain/domain/motion/CMPValidator.h",
         "#include \"../../../../motion-planning/domain/motion/CMPValidator.h\""},
        {repo_root / "modules/workflow/domain/domain/motion/domain-services/GeometryBlender.h",
         "#include \"../../../../../motion-planning/domain/motion/domain-services/GeometryBlender.h\""},
        {repo_root / "modules/workflow/domain/domain/motion/domain-services/SevenSegmentSCurveProfile.h",
         "#include \"../../../../../motion-planning/domain/motion/domain-services/SevenSegmentSCurveProfile.h\""},
        {repo_root / "modules/workflow/domain/domain/motion/domain-services/SpeedPlanner.h",
         "#include \"../../../../../motion-planning/domain/motion/domain-services/SpeedPlanner.h\""},
        {repo_root / "modules/workflow/domain/domain/motion/domain-services/TimeTrajectoryPlanner.h",
         "#include \"../../../../../motion-planning/domain/motion/domain-services/TimeTrajectoryPlanner.h\""},
        {repo_root / "modules/workflow/domain/domain/motion/domain-services/TrajectoryPlanner.h",
         "#include \"../../../../../motion-planning/domain/motion/domain-services/TrajectoryPlanner.h\""},
        {repo_root / "modules/workflow/domain/domain/motion/domain-services/TriggerCalculator.h",
         "#include \"../../../../../motion-planning/domain/motion/domain-services/TriggerCalculator.h\""},
        {repo_root / "modules/workflow/domain/domain/motion/domain-services/VelocityProfileService.h",
         "#include \"../../../../../motion-planning/domain/motion/domain-services/VelocityProfileService.h\""},
        {repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/ArcGeometryMath.h",
         "#include \"../../../../../../motion-planning/domain/motion/domain-services/interpolation/ArcGeometryMath.h\""},
        {repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/ArcInterpolator.h",
         "#include \"../../../../../../motion-planning/domain/motion/domain-services/interpolation/ArcInterpolator.h\""},
        {repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/InterpolationCommandValidator.h",
         "#include \"../../../../../../motion-planning/domain/motion/domain-services/interpolation/InterpolationCommandValidator.h\""},
        {repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/InterpolationProgramPlanner.h",
         "#include \"../../../../../../motion-planning/domain/motion/domain-services/interpolation/InterpolationProgramPlanner.h\""},
        {repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/LinearInterpolator.h",
         "#include \"../../../../../../motion-planning/domain/motion/domain-services/interpolation/LinearInterpolator.h\""},
        {repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/SplineGeometryMath.h",
         "#include \"../../../../../../motion-planning/domain/motion/domain-services/interpolation/SplineGeometryMath.h\""},
        {repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/SplineInterpolator.h",
         "#include \"../../../../../../motion-planning/domain/motion/domain-services/interpolation/SplineInterpolator.h\""},
        {repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/TrajectoryInterpolatorBase.h",
         "#include \"../../../../../../motion-planning/domain/motion/domain-services/interpolation/TrajectoryInterpolatorBase.h\""},
        {repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/ValidatedInterpolationPort.h",
         "#include \"../../../../../../motion-planning/domain/motion/domain-services/interpolation/ValidatedInterpolationPort.h\""},
        {repo_root / "modules/workflow/domain/include/domain/motion/CMPCoordinatedInterpolator.h",
         "#include \"../../../../../motion-planning/domain/motion/CMPCoordinatedInterpolator.h\""},
        {repo_root / "modules/workflow/domain/include/domain/motion/BezierCalculator.h",
         "#include \"../../../../../motion-planning/domain/motion/BezierCalculator.h\""},
        {repo_root / "modules/workflow/domain/include/domain/motion/BSplineCalculator.h",
         "#include \"../../../../../motion-planning/domain/motion/BSplineCalculator.h\""},
        {repo_root / "modules/workflow/domain/include/domain/motion/CircleCalculator.h",
         "#include \"../../../../../motion-planning/domain/motion/CircleCalculator.h\""},
        {repo_root / "modules/workflow/domain/include/domain/motion/CMPCompensation.h",
         "#include \"../../../../../motion-planning/domain/motion/CMPCompensation.h\""},
        {repo_root / "modules/workflow/domain/include/domain/motion/CMPValidator.h",
         "#include \"../../../../../motion-planning/domain/motion/CMPValidator.h\""},
        {repo_root / "modules/workflow/domain/include/domain/motion/domain-services/interpolation/InterpolationProgramPlanner.h",
         "#include \"../../../../../../../motion-planning/domain/motion/domain-services/interpolation/InterpolationProgramPlanner.h\""},
        {repo_root / "modules/workflow/domain/include/domain/motion/domain-services/interpolation/ArcInterpolator.h",
         "#include \"../../../../../../../motion-planning/domain/motion/domain-services/interpolation/ArcInterpolator.h\""},
    }};

    for (const auto& [path, include_line] : expectations) {
        const std::string content = ReadTextFile(path);
        EXPECT_NE(content.find("Canonical planning owner lives in motion-planning"), std::string::npos) << path.string();
        EXPECT_NE(content.find(include_line), std::string::npos) << path.string();
    }
}

TEST(MotionPlanningOwnerBoundaryTest, WorkflowPlanningImplementationsAreRemoved) {
    const fs::path repo_root = RepoRoot();
    const std::array<fs::path, 19> legacy_sources = {{
        repo_root / "modules/workflow/domain/domain/motion/CMPCoordinatedInterpolator.cpp",
        repo_root / "modules/workflow/domain/domain/motion/BezierCalculator.cpp",
        repo_root / "modules/workflow/domain/domain/motion/BSplineCalculator.cpp",
        repo_root / "modules/workflow/domain/domain/motion/CircleCalculator.cpp",
        repo_root / "modules/workflow/domain/domain/motion/CMPCompensation.cpp",
        repo_root / "modules/workflow/domain/domain/motion/CMPValidator.cpp",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/GeometryBlender.cpp",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/SevenSegmentSCurveProfile.cpp",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/SpeedPlanner.cpp",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/TimeTrajectoryPlanner.cpp",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/TrajectoryPlanner.cpp",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/TriggerCalculator.cpp",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/VelocityProfileService.cpp",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/ArcGeometryMath.cpp",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/ArcInterpolator.cpp",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/InterpolationCommandValidator.cpp",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/InterpolationProgramPlanner.cpp",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/LinearInterpolator.cpp",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/TrajectoryInterpolatorBase.cpp",
    }};

    for (const auto& source : legacy_sources) {
        EXPECT_FALSE(fs::exists(source)) << source.string();
    }

    const std::array<fs::path, 3> extra_sources = {{
        repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/SplineGeometryMath.cpp",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/SplineInterpolator.cpp",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/ValidatedInterpolationPort.cpp",
    }};

    for (const auto& source : extra_sources) {
        EXPECT_FALSE(fs::exists(source)) << source.string();
    }
}

TEST(MotionPlanningOwnerBoundaryTest, WorkflowCmpPrecisionTestUsesContractsProcessPathSemantics) {
    const fs::path repo_root = RepoRoot();
    const std::string workflow_cmp_test = ReadTextFile(
        repo_root / "modules/workflow/tests/process-runtime-core/unit/domain/motion/CMPCoordinatedInterpolatorPrecisionTest.cpp");

    EXPECT_NE(workflow_cmp_test.find("using Siligen::ProcessPath::Contracts::ProcessPath;"), std::string::npos);
    EXPECT_NE(workflow_cmp_test.find("using Siligen::ProcessPath::Contracts::ArcPrimitive;"), std::string::npos);
    EXPECT_EQ(workflow_cmp_test.find("using Siligen::Domain::Trajectory::ValueObjects::ProcessPath;"), std::string::npos);
}

TEST(MotionPlanningOwnerBoundaryTest, InterpolationProgramPlannerConsumersUseContractsProcessPathSemantics) {
    const fs::path repo_root = RepoRoot();
    const std::array<fs::path, 4> sources = {{
        repo_root / "modules/motion-planning/tests/unit/domain/trajectory/InterpolationProgramPlannerTest.cpp",
        repo_root / "modules/workflow/application/usecases/motion/trajectory/DeterministicPathExecutionUseCase.cpp",
        repo_root / "modules/workflow/domain/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp",
        repo_root / "modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp",
    }};

    for (const auto& path : sources) {
        const std::string content = ReadTextFile(path);
        EXPECT_NE(content.find("using Siligen::ProcessPath::Contracts::ProcessPath;"), std::string::npos)
            << path.string();
        EXPECT_EQ(content.find("using Siligen::Domain::Trajectory::ValueObjects::ProcessPath;"), std::string::npos)
            << path.string();
    }

    const std::string workflow_interpolation_test = ReadTextFile(sources.front());
    EXPECT_NE(workflow_interpolation_test.find("using Siligen::MotionPlanning::Contracts::TimePlanningConfig;"),
              std::string::npos);
    EXPECT_EQ(workflow_interpolation_test.find("using Siligen::Domain::Trajectory::ValueObjects::MotionConfig;"),
              std::string::npos);
}

TEST(MotionPlanningOwnerBoundaryTest, WorkflowResidualUnifiedTrajectoryPlannerIsRemoved) {
    const fs::path repo_root = RepoRoot();

    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/domain/domain/dispensing/planning/domain-services/UnifiedTrajectoryPlannerService.h"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/domain/domain/dispensing/planning/domain-services/UnifiedTrajectoryPlannerService.cpp"));

    const std::string workflow_dispensing_planner = ReadTextFile(
        repo_root / "modules/workflow/domain/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp");
    EXPECT_NE(workflow_dispensing_planner.find(
                  "../../../../../../../dispense-packaging/domain/dispensing/planning/domain-services/UnifiedTrajectoryPlannerService.h"),
              std::string::npos);
    EXPECT_EQ(workflow_dispensing_planner.find('#' + std::string("include \"UnifiedTrajectoryPlannerService.h\"")),
              std::string::npos);
}

TEST(MotionPlanningOwnerBoundaryTest, WorkflowValueObjectThinBridgesAreRemoved) {
    const fs::path repo_root = RepoRoot();

    const std::array<fs::path, 7> removed_headers = {{
        repo_root / "modules/workflow/domain/include/domain/trajectory/value-objects/Path.h",
        repo_root / "modules/workflow/domain/include/domain/trajectory/value-objects/Primitive.h",
        repo_root / "modules/workflow/domain/include/domain/trajectory/value-objects/ProcessConfig.h",
        repo_root / "modules/workflow/domain/include/domain/trajectory/value-objects/ProcessPath.h",
        repo_root / "modules/workflow/domain/include/domain/trajectory/value-objects/GeometryUtils.h",
        repo_root / "modules/workflow/domain/include/domain/trajectory/value-objects/GeometryBoostAdapter.h",
        repo_root / "modules/workflow/domain/include/domain/trajectory/value-objects/PlanningReport.h",
    }};

    for (const auto& header : removed_headers) {
        EXPECT_FALSE(fs::exists(header)) << header.string();
    }
}
}  // namespace
