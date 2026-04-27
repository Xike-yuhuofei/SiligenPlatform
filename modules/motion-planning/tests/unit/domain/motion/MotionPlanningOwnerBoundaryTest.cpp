#include "domain/motion/CMPCoordinatedInterpolator.h"
#include "domain/motion/CMPCompensation.h"
#include "domain/motion/CMPValidator.h"
#include "domain/motion/CircleCalculator.h"
#include "motion_planning/contracts/InterpolationProgramPlanner.h"
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
    using MotionTrajectory = Siligen::MotionPlanning::Contracts::MotionTrajectory;
    using TimePlanningConfig = Siligen::MotionPlanning::Contracts::TimePlanningConfig;
    using ContractsProcessPath = Siligen::ProcessPath::Contracts::ProcessPath;
    using InterpolationData = Siligen::RuntimeExecution::Contracts::Motion::InterpolationData;
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
    static_assert(std::is_class_v<Siligen::Domain::Motion::CircleCalculator>);
    static_assert(std::is_class_v<Siligen::Domain::Motion::CMPCompensation>);
    static_assert(std::is_class_v<Siligen::Domain::Motion::CMPValidator>);
}

TEST(MotionPlanningOwnerBoundaryTest, WorkflowDomainRequiresCanonicalMotionOwnerTarget) {
    const fs::path repo_root = RepoRoot();
    const fs::path workflow_cmake =
        repo_root / "modules/workflow/domain/domain/CMakeLists.txt";

    EXPECT_FALSE(fs::exists(workflow_cmake)) << workflow_cmake.string();
}

TEST(MotionPlanningOwnerBoundaryTest, MotionPlanningPublicSurfaceDoesNotExportInterpolationUseCase) {
    const fs::path repo_root = RepoRoot();

    EXPECT_FALSE(fs::exists(
        repo_root / "modules/motion-planning/application/include/motion_planning/application/usecases/motion/interpolation/InterpolationPlanningUseCase.h"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/motion-planning/application/usecases/motion/interpolation/InterpolationPlanningUseCase.h"));
    EXPECT_TRUE(fs::exists(
        repo_root / "modules/runtime-execution/application/include/runtime_execution/application/usecases/motion/interpolation/InterpolationPlanningUseCase.h"));
    EXPECT_TRUE(fs::exists(
        repo_root / "modules/runtime-execution/application/usecases/motion/interpolation/InterpolationPlanningUseCase.h"));
}

TEST(MotionPlanningOwnerBoundaryTest, MotionPlanningPublicSurfaceDoesNotExportInterpolationProgramFacade) {
    const fs::path repo_root = RepoRoot();
    const fs::path legacy_header =
        repo_root / "modules/motion-planning/application/include/application/services/motion_planning/InterpolationProgramFacade.h";
    const fs::path legacy_source =
        repo_root / "modules/motion-planning/application/services/motion_planning/InterpolationProgramFacade.cpp";

    EXPECT_FALSE(fs::exists(legacy_header)) << legacy_header.string();
    EXPECT_FALSE(fs::exists(legacy_source)) << legacy_source.string();

    const std::string application_cmake =
        ReadTextFile(repo_root / "modules/motion-planning/application/CMakeLists.txt");
    const std::string readme = ReadTextFile(repo_root / "modules/motion-planning/README.md");
    const std::string module_yaml = ReadTextFile(repo_root / "modules/motion-planning/module.yaml");

    EXPECT_EQ(application_cmake.find("InterpolationProgramFacade.cpp"), std::string::npos);
    EXPECT_EQ(readme.find("InterpolationProgramFacade"), std::string::npos);
    EXPECT_EQ(module_yaml.find("InterpolationProgramFacade"), std::string::npos);
}

TEST(MotionPlanningOwnerBoundaryTest, MotionPlanningContractsExposeThinMotionValueObjectWrappers) {
    const fs::path repo_root = RepoRoot();
    const std::array<std::pair<fs::path, std::string>, 4> expectations = {{
        {repo_root / "modules/motion-planning/contracts/include/motion_planning/contracts/MotionTypes.h",
         "#include \"../../../../domain/motion/value-objects/MotionTypes.h\""},
        {repo_root / "modules/motion-planning/contracts/include/motion_planning/contracts/HardwareTestTypes.h",
         "#include \"../../../../domain/motion/value-objects/HardwareTestTypes.h\""},
        {repo_root / "modules/motion-planning/contracts/include/motion_planning/contracts/TrajectoryTypes.h",
         "#include \"../../../../domain/motion/value-objects/TrajectoryTypes.h\""},
        {repo_root / "modules/motion-planning/contracts/include/motion_planning/contracts/TrajectoryAnalysisTypes.h",
         "#include \"../../../../domain/motion/value-objects/TrajectoryAnalysisTypes.h\""},
    }};

    for (const auto& [path, include_line] : expectations) {
        const std::string content = ReadTextFile(path);
        EXPECT_NE(content.find("Thin public wrapper"), std::string::npos) << path.string();
        EXPECT_NE(content.find(include_line), std::string::npos) << path.string();
    }
}

TEST(MotionPlanningOwnerBoundaryTest, MotionPlanningContractsExposeThinMotionPortWrappers) {
    const fs::path repo_root = RepoRoot();
    const std::array<std::pair<fs::path, std::string>, 2> expectations = {{
        {repo_root / "modules/motion-planning/contracts/include/motion_planning/contracts/IAdvancedMotionPort.h",
         "#include \"../../../../domain/motion/ports/IAdvancedMotionPort.h\""},
        {repo_root / "modules/motion-planning/contracts/include/motion_planning/contracts/IVelocityProfilePort.h",
         "#include \"../../../../domain/motion/ports/IVelocityProfilePort.h\""},
    }};

    for (const auto& [path, include_line] : expectations) {
        const std::string content = ReadTextFile(path);
        EXPECT_NE(content.find("Thin public wrapper"), std::string::npos) << path.string();
        EXPECT_NE(content.find(include_line), std::string::npos) << path.string();
    }
}

TEST(MotionPlanningOwnerBoundaryTest, RuntimeConsumersUseMotionPlanningContractsSurfaceForMotionValueObjects) {
    const fs::path repo_root = RepoRoot();

    const std::string trigger_controller = ReadTextFile(
        repo_root / "modules/runtime-execution/adapters/device/include/siligen/device/adapters/dispensing/TriggerControllerAdapter.h");
    EXPECT_NE(trigger_controller.find('#' + std::string("include \"motion_planning/contracts/HardwareTestTypes.h\"")),
              std::string::npos);
    EXPECT_EQ(trigger_controller.find('#' + std::string("include \"domain/motion/value-objects/HardwareTestTypes.h\"")),
              std::string::npos);

    const std::string homing_support = ReadTextFile(
        repo_root / "modules/runtime-execution/adapters/device/include/siligen/device/adapters/motion/HomingSupport.h");
    EXPECT_NE(homing_support.find('#' + std::string("include \"motion_planning/contracts/HardwareTestTypes.h\"")),
              std::string::npos);
    EXPECT_NE(homing_support.find('#' + std::string("include \"motion_planning/contracts/MotionTypes.h\"")),
              std::string::npos);
    EXPECT_EQ(homing_support.find('#' + std::string("include \"domain/motion/value-objects/HardwareTestTypes.h\"")),
              std::string::npos);
    EXPECT_EQ(homing_support.find('#' + std::string("include \"domain/motion/value-objects/MotionTypes.h\"")),
              std::string::npos);

    const std::string homing_port = ReadTextFile(
        repo_root / "modules/runtime-execution/adapters/device/include/siligen/device/adapters/motion/HomingPortAdapter.h");
    EXPECT_NE(homing_port.find('#' + std::string("include \"motion_planning/contracts/HardwareTestTypes.h\"")),
              std::string::npos);
    EXPECT_EQ(homing_port.find('#' + std::string("include \"domain/motion/value-objects/HardwareTestTypes.h\"")),
              std::string::npos);

    const std::string runtime_homing_contract = ReadTextFile(
        repo_root / "modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/motion/IHomingPort.h");
    EXPECT_NE(runtime_homing_contract.find('#' + std::string("include \"motion_planning/contracts/MotionTypes.h\"")),
              std::string::npos);
    EXPECT_EQ(runtime_homing_contract.find("enum class HomingStage;"), std::string::npos);

    const std::string process_planning_config = ReadTextFile(
        repo_root / "modules/process-planning/domain/configuration/value-objects/ConfigTypes.h");
    EXPECT_NE(process_planning_config.find('#' + std::string("include \"motion_planning/contracts/MotionTypes.h\"")),
              std::string::npos);
    EXPECT_EQ(process_planning_config.find('#' + std::string("include \"domain/motion/value-objects/MotionTypes.h\"")),
              std::string::npos);

    const std::string diagnostics_types = ReadTextFile(
        repo_root / "apps/runtime-service/include/runtime_process_bootstrap/diagnostics/value-objects/TestDataTypes.h");
    EXPECT_NE(diagnostics_types.find('#' + std::string("include \"motion_planning/contracts/HardwareTestTypes.h\"")),
              std::string::npos);
    EXPECT_NE(diagnostics_types.find('#' + std::string("include \"motion_planning/contracts/MotionTypes.h\"")),
              std::string::npos);
    EXPECT_NE(
        diagnostics_types.find('#' + std::string("include \"motion_planning/contracts/TrajectoryAnalysisTypes.h\"")),
        std::string::npos);
    EXPECT_NE(diagnostics_types.find('#' + std::string("include \"motion_planning/contracts/TrajectoryTypes.h\"")),
              std::string::npos);
    EXPECT_EQ(diagnostics_types.find('#' + std::string("include \"domain/motion/value-objects/HardwareTestTypes.h\"")),
              std::string::npos);
    EXPECT_EQ(diagnostics_types.find('#' + std::string("include \"domain/motion/value-objects/MotionTypes.h\"")),
              std::string::npos);
    EXPECT_EQ(
        diagnostics_types.find('#' + std::string("include \"domain/motion/value-objects/TrajectoryAnalysisTypes.h\"")),
        std::string::npos);
    EXPECT_EQ(diagnostics_types.find('#' + std::string("include \"domain/motion/value-objects/TrajectoryTypes.h\"")),
              std::string::npos);
}

TEST(MotionPlanningOwnerBoundaryTest, RuntimeTargetsDependOnMotionPlanningContractsInsteadOfRawIncludeRoots) {
    const fs::path repo_root = RepoRoot();

    const std::string device_cmake =
        ReadTextFile(repo_root / "modules/runtime-execution/adapters/device/CMakeLists.txt");
    EXPECT_NE(device_cmake.find("siligen_motion_planning_contracts_public"), std::string::npos);
    EXPECT_EQ(device_cmake.find("SILIGEN_DEVICE_ADAPTERS_MOTION_PLANNING_INCLUDE_ROOT"), std::string::npos);
    EXPECT_EQ(device_cmake.find("\"${SILIGEN_WORKSPACE_ROOT}/modules/motion-planning\""), std::string::npos);

    const std::string process_planning_cmake =
        ReadTextFile(repo_root / "modules/process-planning/domain/configuration/CMakeLists.txt");
    EXPECT_NE(process_planning_cmake.find("siligen_motion_planning_contracts_public"), std::string::npos);
    EXPECT_NE(process_planning_cmake.find("../../motion-planning/contracts"), std::string::npos);

    const std::string runtime_tests_cmake =
        ReadTextFile(repo_root / "modules/runtime-execution/tests/unit/CMakeLists.txt");
    EXPECT_EQ(runtime_tests_cmake.find("modules/motion-planning/contracts/include"), std::string::npos);
}

TEST(MotionPlanningOwnerBoundaryTest, MotionPlanningApplicationExposesThinPlanningWrappers) {
    const fs::path repo_root = RepoRoot();
    const std::array<std::pair<fs::path, std::string>, 3> expectations = {{
        {repo_root / "modules/motion-planning/application/include/application/services/motion_planning/VelocityProfileService.h",
         "#include \"../../../../../domain/motion/domain-services/VelocityProfileService.h\""},
        {repo_root / "modules/motion-planning/application/include/application/services/motion_planning/SevenSegmentSCurveProfile.h",
         "#include \"../../../../../domain/motion/domain-services/SevenSegmentSCurveProfile.h\""},
        {repo_root / "modules/motion-planning/application/include/application/services/motion_planning/InterpolationCommandValidator.h",
         "#include \"../../../../../domain/motion/domain-services/interpolation/InterpolationCommandValidator.h\""},
    }};

    for (const auto& [path, include_line] : expectations) {
        const std::string content = ReadTextFile(path);
        EXPECT_NE(content.find("Thin public wrapper"), std::string::npos) << path.string();
        EXPECT_NE(content.find(include_line), std::string::npos) << path.string();
    }
}

TEST(MotionPlanningOwnerBoundaryTest, RuntimeConsumersUseMotionPlanningPublicSurfaceForVelocityPlanning) {
    const fs::path repo_root = RepoRoot();

    const std::string container_bootstrap =
        ReadTextFile(repo_root / "apps/runtime-service/bootstrap/ContainerBootstrap.cpp");
    EXPECT_NE(container_bootstrap.find('#' + std::string("include \"motion_planning/contracts/IVelocityProfilePort.h\"")),
              std::string::npos);
    EXPECT_EQ(container_bootstrap.find('#' + std::string("include \"domain/motion/ports/IVelocityProfilePort.h\"")),
              std::string::npos);

    const std::string adapter_factory_header =
        ReadTextFile(repo_root / "apps/runtime-service/factories/InfrastructureAdapterFactory.h");
    EXPECT_NE(adapter_factory_header.find('#' + std::string("include \"motion_planning/contracts/IVelocityProfilePort.h\"")),
              std::string::npos);
    EXPECT_EQ(adapter_factory_header.find('#' + std::string("include \"domain/motion/ports/IVelocityProfilePort.h\"")),
              std::string::npos);

    const std::string bindings_builder =
        ReadTextFile(repo_root / "apps/runtime-service/bootstrap/InfrastructureBindingsBuilder.cpp");
    EXPECT_NE(bindings_builder.find('#' + std::string("include \"application/services/motion_planning/SevenSegmentSCurveProfile.h\"")),
              std::string::npos);
    EXPECT_EQ(bindings_builder.find('#' + std::string("include \"domain/motion/domain-services/SevenSegmentSCurveProfile.h\"")),
              std::string::npos);

    const std::string adapter_factory_cpp =
        ReadTextFile(repo_root / "apps/runtime-service/factories/InfrastructureAdapterFactory.cpp");
    EXPECT_NE(adapter_factory_cpp.find('#' + std::string("include \"application/services/motion_planning/SevenSegmentSCurveProfile.h\"")),
              std::string::npos);
    EXPECT_EQ(adapter_factory_cpp.find('#' + std::string("include \"domain/motion/domain-services/SevenSegmentSCurveProfile.h\"")),
              std::string::npos);

    const std::string application_container =
        ReadTextFile(repo_root / "apps/runtime-service/container/ApplicationContainer.Motion.cpp");
    EXPECT_NE(application_container.find('#' + std::string("include \"application/services/motion_planning/VelocityProfileService.h\"")),
              std::string::npos);
    EXPECT_EQ(application_container.find('#' + std::string("include \"domain/motion/domain-services/VelocityProfileService.h\"")),
              std::string::npos);

    const std::string coordination_use_case =
        ReadTextFile(repo_root / "modules/runtime-execution/application/include/runtime_execution/application/usecases/motion/coordination/MotionCoordinationUseCase.h");
    EXPECT_NE(coordination_use_case.find('#' + std::string("include \"motion_planning/contracts/IAdvancedMotionPort.h\"")),
              std::string::npos);
    EXPECT_EQ(coordination_use_case.find('#' + std::string("include \"domain/motion/ports/IAdvancedMotionPort.h\"")),
              std::string::npos);
    EXPECT_EQ(coordination_use_case.find('#' + std::string("include \"modules/motion-planning/domain/motion/ports/IAdvancedMotionPort.h\"")),
              std::string::npos);
}

TEST(MotionPlanningOwnerBoundaryTest, RuntimeExecutionInterpolationValidationUsesMotionPlanningApplicationSurface) {
    const fs::path repo_root = RepoRoot();

    const std::string validated_port = ReadTextFile(
        repo_root / "modules/runtime-execution/application/services/motion/interpolation/ValidatedInterpolationPort.cpp");
    EXPECT_NE(
        validated_port.find('#' + std::string("include \"application/services/motion_planning/InterpolationCommandValidator.h\"")),
        std::string::npos);
    EXPECT_EQ(
        validated_port.find(
            '#' + std::string("include \"domain/motion/domain-services/interpolation/InterpolationCommandValidator.h\"")),
        std::string::npos);

    const std::string runtime_cmake =
        ReadTextFile(repo_root / "modules/runtime-execution/application/CMakeLists.txt");
    const auto application_private_link =
        runtime_cmake.find("target_link_libraries(siligen_runtime_execution_application PRIVATE");
    EXPECT_NE(application_private_link, std::string::npos);
    EXPECT_NE(runtime_cmake.find("siligen_motion_planning_application_public", application_private_link),
              std::string::npos);
}

TEST(MotionPlanningOwnerBoundaryTest, WorkflowVelocityPlanningCompatibilityHeadersAreRemoved) {
    const fs::path repo_root = RepoRoot();
    const std::array<fs::path, 4> removed_headers = {{
        repo_root / "modules/workflow/domain/include/domain/motion/ports/IAdvancedMotionPort.h",
        repo_root / "modules/workflow/domain/include/domain/motion/ports/IVelocityProfilePort.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/VelocityProfileService.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/SevenSegmentSCurveProfile.h",
    }};

    for (const auto& header : removed_headers) {
        EXPECT_FALSE(fs::exists(header)) << header.string();
    }
}

TEST(MotionPlanningOwnerBoundaryTest, WorkflowPlanningCompatibilityHeadersAreRemovedFromPublicIncludeRoot) {
    const fs::path repo_root = RepoRoot();
    const std::array<fs::path, 22> removed_headers = {{
        repo_root / "modules/workflow/domain/include/domain/motion/BezierCalculator.h",
        repo_root / "modules/workflow/domain/include/domain/motion/BSplineCalculator.h",
        repo_root / "modules/workflow/domain/include/domain/motion/CircleCalculator.h",
        repo_root / "modules/workflow/domain/include/domain/motion/CMPCompensation.h",
        repo_root / "modules/workflow/domain/include/domain/motion/CMPValidator.h",
        repo_root / "modules/workflow/domain/include/domain/motion/value-objects/HardwareTestTypes.h",
        repo_root / "modules/workflow/domain/include/domain/motion/value-objects/MotionTypes.h",
        repo_root / "modules/workflow/domain/include/domain/motion/value-objects/SemanticPath.h",
        repo_root / "modules/workflow/domain/include/domain/motion/value-objects/TrajectoryAnalysisTypes.h",
        repo_root / "modules/workflow/domain/include/domain/motion/value-objects/TrajectoryTypes.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/GeometryBlender.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/MotionPlanner.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/TrajectoryPlanner.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/TriggerCalculator.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/interpolation/ArcGeometryMath.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/interpolation/ArcInterpolator.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/interpolation/InterpolationCommandValidator.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/interpolation/LinearInterpolator.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/interpolation/SplineGeometryMath.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/interpolation/SplineInterpolator.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/interpolation/ValidatedInterpolationPort.h",
    }};

    for (const auto& header : removed_headers) {
        EXPECT_FALSE(fs::exists(header)) << header.string();
    }
}

TEST(MotionPlanningOwnerBoundaryTest, WorkflowDormantPlanningShellHeadersAreRemoved) {
    const fs::path repo_root = RepoRoot();
    const std::array<fs::path, 29> removed_headers = {{
        repo_root / "modules/workflow/domain/domain/motion/BezierCalculator.h",
        repo_root / "modules/workflow/domain/domain/motion/BSplineCalculator.h",
        repo_root / "modules/workflow/domain/domain/motion/CircleCalculator.h",
        repo_root / "modules/workflow/domain/domain/motion/CMPCompensation.h",
        repo_root / "modules/workflow/domain/domain/motion/CMPValidator.h",
        repo_root / "modules/workflow/domain/domain/motion/value-objects/HardwareTestTypes.h",
        repo_root / "modules/workflow/domain/domain/motion/value-objects/MotionTypes.h",
        repo_root / "modules/workflow/domain/domain/motion/value-objects/SemanticPath.h",
        repo_root / "modules/workflow/domain/domain/motion/value-objects/TrajectoryAnalysisTypes.h",
        repo_root / "modules/workflow/domain/domain/motion/value-objects/TrajectoryTypes.h",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/GeometryBlender.h",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/TrajectoryPlanner.h",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/TriggerCalculator.h",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/ArcGeometryMath.h",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/ArcInterpolator.h",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/InterpolationCommandValidator.h",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/LinearInterpolator.h",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/SplineGeometryMath.h",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/SplineInterpolator.h",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/ValidatedInterpolationPort.h",
        repo_root / "modules/workflow/domain/domain/motion/CMPCoordinatedInterpolator.h",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/TimeTrajectoryPlanner.h",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/InterpolationProgramPlanner.h",
        repo_root / "modules/workflow/domain/domain/motion/domain-services/interpolation/TrajectoryInterpolatorBase.h",
        repo_root / "modules/workflow/domain/include/domain/motion/CMPCoordinatedInterpolator.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/TimeTrajectoryPlanner.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/interpolation/InterpolationProgramPlanner.h",
        repo_root / "modules/workflow/domain/include/domain/motion/domain-services/interpolation/TrajectoryInterpolatorBase.h",
    }};

    for (const auto& header : removed_headers) {
        EXPECT_FALSE(fs::exists(header)) << header.string();
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

TEST(MotionPlanningOwnerBoundaryTest, WorkflowTrajectoryMotionPlannerResidueIsRemoved) {
    const fs::path repo_root = RepoRoot();
    const std::array<fs::path, 5> legacy_paths = {{
        repo_root / "modules/workflow/domain/domain/trajectory/domain-services/MotionPlanner.cpp",
        repo_root / "modules/workflow/domain/domain/trajectory/domain-services/MotionPlanner.h",
        repo_root / "modules/workflow/domain/domain/trajectory/value-objects/MotionConfig.h",
        repo_root / "modules/workflow/domain/domain/trajectory/value-objects/PlanningReport.h",
        repo_root / "modules/workflow/domain/include/domain/trajectory/value-objects/PlanningReport.h",
    }};

    for (const auto& path : legacy_paths) {
        EXPECT_FALSE(fs::exists(path)) << path.string();
    }
}

TEST(MotionPlanningOwnerBoundaryTest, WorkflowTriggerCmpCompatibilityIsShimOnly) {
    const fs::path repo_root = RepoRoot();
    const std::string workflow_domain_root_cmake =
        ReadTextFile(repo_root / "modules/workflow/domain/CMakeLists.txt");
    EXPECT_EQ(workflow_domain_root_cmake.find("triggering"), std::string::npos);
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/domain/include/domain/dispensing/domain-services/CMPTriggerService.h"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/domain/domain/dispensing/domain-services/TriggerPlanner.h"));
}

TEST(MotionPlanningOwnerBoundaryTest, WorkflowCmpPrecisionResidueIsRemovedFromWorkflowModule) {
    const fs::path repo_root = RepoRoot();
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/tests/process-runtime-core/unit/domain/motion/CMPCoordinatedInterpolatorPrecisionTest.cpp"));
}

TEST(MotionPlanningOwnerBoundaryTest, InterpolationProgramPlannerConsumersUseContractsProcessPathSemantics) {
    const fs::path repo_root = RepoRoot();
    const fs::path migrated_owner =
        repo_root / "modules/motion-planning/application/usecases/motion/trajectory/DeterministicPathExecutionUseCase.cpp";
    const fs::path legacy_owner =
        repo_root / "modules/runtime-execution/application/usecases/motion/trajectory/DeterministicPathExecutionUseCase.cpp";

    EXPECT_TRUE(fs::exists(migrated_owner)) << migrated_owner.string();
    EXPECT_FALSE(fs::exists(legacy_owner)) << legacy_owner.string();

    const std::array<fs::path, 4> planner_sources = {{
        repo_root / "modules/motion-planning/tests/unit/domain/trajectory/InterpolationProgramPlannerTest.cpp",
        migrated_owner,
        repo_root / "modules/dispense-packaging/application/services/dispensing/PlanningAssemblyExecutionInterpolation.cpp",
        repo_root / "modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp",
    }};

    for (const auto& path : planner_sources) {
        const std::string content = ReadTextFile(path);
        EXPECT_NE(content.find("InterpolationProgramPlanner"), std::string::npos)
            << path.string();
        EXPECT_EQ(content.find("InterpolationProgramFacade"), std::string::npos)
            << path.string();
    }

    const std::array<fs::path, 4> process_path_sources = {{
        repo_root / "modules/motion-planning/tests/unit/domain/trajectory/InterpolationProgramPlannerTest.cpp",
        migrated_owner,
        repo_root / "modules/dispense-packaging/application/services/dispensing/PlanningAssemblyInternals.h",
        repo_root / "modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp",
    }};

    for (const auto& path : process_path_sources) {
        const std::string content = ReadTextFile(path);
        EXPECT_NE(content.find("using Siligen::ProcessPath::Contracts::ProcessPath;"), std::string::npos)
            << path.string();
        EXPECT_EQ(content.find("using Siligen::Domain::Trajectory::ValueObjects::ProcessPath;"), std::string::npos)
            << path.string();
    }

    const std::string workflow_interpolation_test = ReadTextFile(planner_sources.front());
    EXPECT_NE(workflow_interpolation_test.find("using Siligen::MotionPlanning::Contracts::TimePlanningConfig;"),
              std::string::npos);
    EXPECT_EQ(workflow_interpolation_test.find("using Siligen::Domain::Trajectory::ValueObjects::MotionConfig;"),
              std::string::npos);
}

TEST(MotionPlanningOwnerBoundaryTest, WorkflowPlanningCompatibilityStubsAreDeleted) {
    const fs::path repo_root = RepoRoot();

    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/domain/domain/dispensing/planning/domain-services/UnifiedTrajectoryPlannerService.h"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/domain/domain/dispensing/planning/domain-services/UnifiedTrajectoryPlannerService.cpp"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/domain/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/domain/domain/dispensing/planning/domain-services/ContourOptimizationService.cpp"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/domain/domain/dispensing/planning/domain-services/DispensingPlannerService.h"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/domain/domain/dispensing/planning/domain-services/ContourOptimizationService.h"));
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
