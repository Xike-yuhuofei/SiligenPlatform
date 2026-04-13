#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

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

std::size_t CountOccurrences(const std::string& content, const std::string& needle) {
    std::size_t count = 0;
    std::size_t cursor = 0;
    while ((cursor = content.find(needle, cursor)) != std::string::npos) {
        ++count;
        cursor += needle.size();
    }
    return count;
}

std::string ExtractBlock(const std::string& content, const std::string& start_token) {
    const std::size_t start = content.find(start_token);
    EXPECT_NE(start, std::string::npos) << "missing token: " << start_token;
    if (start == std::string::npos) {
        return {};
    }

    const std::size_t end = content.find("\n)", start);
    EXPECT_NE(end, std::string::npos) << "unterminated block: " << start_token;
    if (end == std::string::npos) {
        return {};
    }

    return content.substr(start, end - start);
}

TEST(DispensePackagingResidualAcceptanceTest, UnifiedTrajectoryPlannerWrapperIsDeletedAndOwnerTestsCarryCoverage) {
    const fs::path repo_root = RepoRoot();
    const fs::path wrapper_header =
        repo_root / "modules/dispense-packaging/domain/dispensing/planning/domain-services/UnifiedTrajectoryPlannerService.h";
    const fs::path wrapper_source =
        repo_root / "modules/dispense-packaging/domain/dispensing/planning/domain-services/UnifiedTrajectoryPlannerService.cpp";
    const std::string planner_service = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp");
    const std::string dispense_packaging_tests = ReadTextFile(
        repo_root / "modules/dispense-packaging/tests/CMakeLists.txt");
    const std::string process_path_tests = ReadTextFile(
        repo_root / "modules/process-path/tests/CMakeLists.txt");
    const std::string process_path_unit = ReadTextFile(
        repo_root / "modules/process-path/tests/unit/application/services/process_path/ProcessPathFacadeTest.cpp");
    const std::string process_path_integration = ReadTextFile(
        repo_root / "modules/process-path/tests/integration/ProcessPathToMotionPlanningIntegrationTest.cpp");

    EXPECT_FALSE(fs::exists(wrapper_header));
    EXPECT_FALSE(fs::exists(wrapper_source));
    EXPECT_NE(
        planner_service.find('#' + std::string("include \"application/services/process_path/ProcessPathFacade.h\"")),
        std::string::npos);
    EXPECT_NE(planner_service.find("BuildProcessPathMotionArtifacts("), std::string::npos);
    EXPECT_NE(planner_service.find("ProcessPathFacade process_path_facade;"), std::string::npos);
    EXPECT_NE(planner_service.find("MotionPlanningFacade motion_planning_facade"), std::string::npos);
    EXPECT_EQ(planner_service.find("UnifiedTrajectoryPlannerService"), std::string::npos);
    EXPECT_EQ(
        planner_service.find('#' + std::string("include \"domain-services/GeometryNormalizer.h\"")),
        std::string::npos);
    EXPECT_EQ(
        planner_service.find('#' + std::string("include \"domain-services/ProcessAnnotator.h\"")),
        std::string::npos);
    EXPECT_EQ(
        planner_service.find('#' + std::string("include \"domain-services/TrajectoryShaper.h\"")),
        std::string::npos);

    EXPECT_EQ(dispense_packaging_tests.find("UnifiedTrajectoryPlannerService.cpp"), std::string::npos);
    EXPECT_EQ(dispense_packaging_tests.find("UnifiedTrajectoryPlannerServiceTest.cpp"), std::string::npos);

    EXPECT_NE(process_path_tests.find("add_executable(siligen_process_path_unit_tests"), std::string::npos);
    EXPECT_NE(process_path_unit.find("BuildsNormalizedAnnotatedAndShapedPath"), std::string::npos);
    EXPECT_NE(process_path_unit.find("EmptyPrimitiveInputReturnsInvalidInputStatus"), std::string::npos);
    EXPECT_NE(process_path_unit.find("RejectsSplineInputWhenApproximationIsDisabled"), std::string::npos);
    EXPECT_NE(process_path_integration.find("PlansMotionTrajectoryFromShapedProcessPath"), std::string::npos);
}

TEST(DispensePackagingResidualAcceptanceTest, ContourOptimizationResidualShrinksToSingleQuarantineSeam) {
    const fs::path repo_root = RepoRoot();
    const fs::path strategy_header =
        repo_root / "modules/dispense-packaging/domain/dispensing/domain-services/PathOptimizationStrategy.h";
    const fs::path strategy_source =
        repo_root / "modules/dispense-packaging/domain/dispensing/domain-services/PathOptimizationStrategy.cpp";
    const std::string contour_service = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/planning/domain-services/ContourOptimizationService.cpp");
    const std::string planner_service = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp");
    const std::string dispense_packaging_tests = ReadTextFile(
        repo_root / "modules/dispense-packaging/tests/CMakeLists.txt");

    EXPECT_FALSE(fs::exists(strategy_header));
    EXPECT_FALSE(fs::exists(strategy_source));
    EXPECT_EQ(
        contour_service.find('#' + std::string("include \"domain/dispensing/domain-services/PathOptimizationStrategy.h\"")),
        std::string::npos);
    EXPECT_NE(contour_service.find("struct SegmentWithDirection"), std::string::npos);
    EXPECT_NE(contour_service.find("OptimizeByNearestNeighbor("), std::string::npos);
    EXPECT_NE(contour_service.find("TwoOptImprove("), std::string::npos);
    EXPECT_EQ(contour_service.find("PathOptimizationStrategy optimizer"), std::string::npos);

    EXPECT_NE(planner_service.find("ContourOptimizationService::Optimize("), std::string::npos);
    EXPECT_EQ(dispense_packaging_tests.find("PathOptimizationStrategy.cpp"), std::string::npos);
    EXPECT_NE(dispense_packaging_tests.find("ContourOptimizationService.cpp"), std::string::npos);
    EXPECT_NE(dispense_packaging_tests.find("DispensingPlannerService.cpp"), std::string::npos);
}

TEST(DispensePackagingResidualAcceptanceTest, DomainDispensingShrinksToThinOwnerCoreAndSplitsResidualHeaderBridgeByIntent) {
    const fs::path repo_root = RepoRoot();
    const std::string content = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/CMakeLists.txt");

    const std::string include_block = ExtractBlock(
        content,
        "target_include_directories(siligen_dispense_packaging_domain_dispensing BEFORE INTERFACE");
    const std::string link_block = ExtractBlock(
        content,
        "target_link_libraries(siligen_dispense_packaging_domain_dispensing INTERFACE");
    const std::string planning_residual_include_block = ExtractBlock(
        content,
        "target_include_directories(siligen_dispense_packaging_domain_dispensing_planning_residual_headers BEFORE INTERFACE");
    const std::string planning_residual_link_block = ExtractBlock(
        content,
        "target_link_libraries(siligen_dispense_packaging_domain_dispensing_planning_residual_headers INTERFACE");
    const std::string execution_residual_include_block = ExtractBlock(
        content,
        "target_include_directories(siligen_dispense_packaging_domain_dispensing_execution_residual_headers BEFORE INTERFACE");
    const std::string execution_residual_link_block = ExtractBlock(
        content,
        "target_link_libraries(siligen_dispense_packaging_domain_dispensing_execution_residual_headers INTERFACE");

    EXPECT_EQ(include_block.find("SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR"), std::string::npos);
    EXPECT_EQ(include_block.find("SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR"), std::string::npos);
    EXPECT_EQ(include_block.find("SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR"), std::string::npos);
    EXPECT_EQ(link_block.find("siligen_runtime_execution_runtime_contracts"), std::string::npos);
    EXPECT_EQ(link_block.find("SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR"), std::string::npos);
    EXPECT_EQ(link_block.find("SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR"), std::string::npos);
    EXPECT_EQ(link_block.find("siligen_workflow_domain_public"), std::string::npos);
    EXPECT_EQ(link_block.find("siligen_workflow_domain_headers"), std::string::npos);
    EXPECT_EQ(link_block.find("siligen_motion_planning_contracts_public"), std::string::npos);
    EXPECT_EQ(link_block.find("siligen_process_path_contracts_public"), std::string::npos);
    EXPECT_EQ(link_block.find("siligen_process_planning_contracts_public"), std::string::npos);
    EXPECT_EQ(link_block.find("siligen_device_contracts"), std::string::npos);
    EXPECT_NE(link_block.find("siligen_dispense_packaging_contracts_public"), std::string::npos);
    EXPECT_NE(link_block.find("siligen_types"), std::string::npos);
    EXPECT_NE(link_block.find("siligen_utils"), std::string::npos);

    EXPECT_NE(
        content.find("add_library(siligen_dispense_packaging_domain_dispensing_planning_residual_headers INTERFACE)"),
        std::string::npos);
    EXPECT_NE(
        content.find("add_library(siligen_dispense_packaging_domain_dispensing_execution_residual_headers INTERFACE)"),
        std::string::npos);
    EXPECT_EQ(
        content.find("add_library(siligen_dispense_packaging_domain_dispensing_residual_headers INTERFACE)"),
        std::string::npos);
    EXPECT_NE(planning_residual_include_block.find("SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR"), std::string::npos);
    EXPECT_NE(
        planning_residual_link_block.find("siligen_dispense_packaging_domain_dispensing"),
        std::string::npos);
    EXPECT_NE(
        planning_residual_link_block.find("siligen_runtime_execution_runtime_contracts"),
        std::string::npos);
    EXPECT_EQ(
        planning_residual_link_block.find("siligen_process_planning_contracts_public"),
        std::string::npos);
    EXPECT_EQ(
        planning_residual_link_block.find("siligen_device_contracts"),
        std::string::npos);
    EXPECT_NE(
        execution_residual_include_block.find("SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR"),
        std::string::npos);
    EXPECT_NE(
        execution_residual_link_block.find("siligen_dispense_packaging_domain_dispensing"),
        std::string::npos);
    EXPECT_NE(
        execution_residual_link_block.find("siligen_runtime_execution_runtime_contracts"),
        std::string::npos);
    EXPECT_NE(
        execution_residual_link_block.find("siligen_process_planning_contracts_public"),
        std::string::npos);
    EXPECT_EQ(content.find("GuardDecision bridge headers"), std::string::npos);
}

TEST(DispensePackagingResidualAcceptanceTest, DispensePackagingOwnerTargetShrinksToCoreAndResidualTargetsCarrySplitHeaderLinks) {
    const fs::path repo_root = RepoRoot();
    const std::string content = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/CMakeLists.txt");
    const std::string domain_link_block = ExtractBlock(
        content,
        "target_link_libraries(siligen_dispense_packaging_domain_dispensing INTERFACE");
    const std::string planning_header_link_block = ExtractBlock(
        content,
        "target_link_libraries(siligen_dispense_packaging_domain_dispensing_planning_residual_headers INTERFACE");
    const std::string execution_header_link_block = ExtractBlock(
        content,
        "target_link_libraries(siligen_dispense_packaging_domain_dispensing_execution_residual_headers INTERFACE");
    const std::string planning_owner_link_block = ExtractBlock(
        content,
        "target_link_libraries(siligen_dispense_packaging_planning_owner_residual PUBLIC");
    const std::string execution_link_block = ExtractBlock(
        content,
        "target_link_libraries(siligen_dispense_packaging_execution_residual PUBLIC");

    EXPECT_NE(content.find("add_library(siligen_dispense_packaging_domain_dispensing INTERFACE)"), std::string::npos);
    EXPECT_NE(
        content.find("add_library(siligen_dispense_packaging_domain_dispensing_planning_residual_headers INTERFACE)"),
        std::string::npos);
    EXPECT_NE(
        content.find("add_library(siligen_dispense_packaging_domain_dispensing_execution_residual_headers INTERFACE)"),
        std::string::npos);
    EXPECT_EQ(
        content.find("add_library(siligen_dispense_packaging_domain_dispensing_residual_headers INTERFACE)"),
        std::string::npos);
    EXPECT_NE(
        content.find("add_library(siligen_dispense_packaging_planning_owner_residual STATIC"),
        std::string::npos);
    EXPECT_EQ(
        content.find("add_library(siligen_dispense_packaging_planning_legacy_dxf_residual STATIC"),
        std::string::npos);
    EXPECT_EQ(content.find("add_library(siligen_dispense_packaging_planning_residual STATIC"), std::string::npos);
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/dispense-packaging/domain/dispensing/PlanningResidualCompat.cpp"));
    EXPECT_NE(content.find("add_library(siligen_dispense_packaging_execution_residual STATIC"), std::string::npos);
    EXPECT_NE(planning_owner_link_block.find("siligen_process_path_contracts_public"), std::string::npos);
    EXPECT_EQ(planning_owner_link_block.find("siligen_process_path_application_public"), std::string::npos);
    EXPECT_EQ(
        planning_owner_link_block.find("${SILIGEN_DISPENSE_PACKAGING_MOTION_PLANNING_APP_TARGET}"),
        std::string::npos);
    EXPECT_EQ(content.find("siligen_process_path_domain_trajectory"), std::string::npos);
    EXPECT_EQ(content.find("target_link_libraries(siligen_dispense_packaging_domain_dispensing PUBLIC"), std::string::npos);
    EXPECT_EQ(domain_link_block.find("siligen_process_path_application_public"), std::string::npos);
    EXPECT_EQ(domain_link_block.find("siligen_motion_planning_application_public"), std::string::npos);
    EXPECT_EQ(domain_link_block.find("${SILIGEN_DISPENSE_PACKAGING_MOTION_PLANNING_APP_TARGET}"), std::string::npos);
    EXPECT_EQ(domain_link_block.find("siligen_runtime_execution_runtime_contracts"), std::string::npos);
    EXPECT_EQ(domain_link_block.find("siligen_process_planning_contracts_public"), std::string::npos);
    EXPECT_EQ(domain_link_block.find("siligen_device_contracts"), std::string::npos);
    EXPECT_NE(
        planning_header_link_block.find("siligen_runtime_execution_runtime_contracts"),
        std::string::npos);
    EXPECT_EQ(
        planning_header_link_block.find("siligen_process_planning_contracts_public"),
        std::string::npos);
    EXPECT_NE(
        planning_owner_link_block.find("siligen_dispense_packaging_domain_dispensing_planning_residual_headers"),
        std::string::npos);
    EXPECT_NE(
        execution_header_link_block.find("siligen_runtime_execution_runtime_contracts"),
        std::string::npos);
    EXPECT_NE(
        execution_header_link_block.find("siligen_process_planning_contracts_public"),
        std::string::npos);
    EXPECT_NE(
        execution_link_block.find("siligen_dispense_packaging_domain_dispensing_execution_residual_headers"),
        std::string::npos);
}

TEST(DispensePackagingResidualAcceptanceTest, DomainPortShimsRemainResidualOnlyWhilePublicCompatHeadersPointAtRuntimeOwners) {
    const fs::path repo_root = RepoRoot();
    const std::string contract_valve = ReadTextFile(
        repo_root / "modules/dispense-packaging/contracts/include/dispense_packaging/contracts/IValvePort.h");
    const std::string contract_trigger = ReadTextFile(
        repo_root / "modules/dispense-packaging/contracts/include/dispense_packaging/contracts/ITriggerControllerPort.h");
    const std::string contract_scheduler = ReadTextFile(
        repo_root / "modules/dispense-packaging/contracts/include/dispense_packaging/contracts/ITaskSchedulerPort.h");
    const std::string contract_observer = ReadTextFile(
        repo_root / "modules/dispense-packaging/contracts/include/dispense_packaging/contracts/IDispensingExecutionObserver.h");
    const std::string domain_valve = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/ports/IValvePort.h");
    const std::string domain_trigger = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/ports/ITriggerControllerPort.h");
    const std::string domain_scheduler = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/ports/ITaskSchedulerPort.h");
    const std::string domain_observer = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/ports/IDispensingExecutionObserver.h");

    EXPECT_NE(contract_valve.find("Compatibility forwarder"), std::string::npos);
    EXPECT_NE(contract_trigger.find("Compatibility forwarder"), std::string::npos);
    EXPECT_NE(contract_scheduler.find("Compatibility forwarder"), std::string::npos);
    EXPECT_NE(contract_observer.find("Compatibility forwarder"), std::string::npos);
    EXPECT_NE(contract_valve.find("runtime_execution/contracts/dispensing/IValvePort.h"), std::string::npos);
    EXPECT_NE(contract_trigger.find("runtime_execution/contracts/dispensing/ITriggerControllerPort.h"), std::string::npos);
    EXPECT_NE(contract_scheduler.find("runtime_execution/contracts/dispensing/ITaskSchedulerPort.h"), std::string::npos);
    EXPECT_NE(
        contract_observer.find("runtime_execution/contracts/dispensing/IDispensingExecutionObserver.h"),
        std::string::npos);
    EXPECT_EQ(contract_valve.find("domain/dispensing/ports/IValvePort.h"), std::string::npos);
    EXPECT_EQ(contract_trigger.find("domain/dispensing/ports/ITriggerControllerPort.h"), std::string::npos);
    EXPECT_EQ(contract_scheduler.find("domain/dispensing/ports/ITaskSchedulerPort.h"), std::string::npos);
    EXPECT_EQ(
        contract_observer.find("domain/dispensing/ports/IDispensingExecutionObserver.h"),
        std::string::npos);

    EXPECT_NE(domain_valve.find("Compatibility shim"), std::string::npos);
    EXPECT_NE(domain_trigger.find("Compatibility shim"), std::string::npos);
    EXPECT_NE(domain_scheduler.find("Compatibility shim"), std::string::npos);
    EXPECT_NE(domain_observer.find("Compatibility shim"), std::string::npos);
    EXPECT_NE(domain_valve.find("runtime_execution/contracts/dispensing/IValvePort.h"), std::string::npos);
    EXPECT_NE(domain_trigger.find("runtime_execution/contracts/dispensing/ITriggerControllerPort.h"), std::string::npos);
    EXPECT_NE(domain_scheduler.find("runtime_execution/contracts/dispensing/ITaskSchedulerPort.h"), std::string::npos);
    EXPECT_NE(
        domain_observer.find("runtime_execution/contracts/dispensing/IDispensingExecutionObserver.h"),
        std::string::npos);
}

TEST(DispensePackagingResidualAcceptanceTest, ApplicationPlanningResidualTargetLocalizesPlanningConcreteLinks) {
    const fs::path repo_root = RepoRoot();
    const std::string content = ReadTextFile(
        repo_root / "modules/dispense-packaging/application/CMakeLists.txt");
    const std::string application_source_block = ExtractBlock(
        content,
        "add_library(siligen_dispense_packaging_application STATIC");
    const std::string planning_common_source_block = ExtractBlock(
        content,
        "add_library(siligen_dispense_packaging_application_planning_common_residual STATIC");
    const std::string planning_authority_source_block = ExtractBlock(
        content,
        "add_library(siligen_dispense_packaging_application_planning_authority_residual STATIC");
    const std::string planning_execution_source_block = ExtractBlock(
        content,
        "add_library(siligen_dispense_packaging_application_planning_execution_residual STATIC");
    const std::string planning_residual_source_block = ExtractBlock(
        content,
        "add_library(siligen_dispense_packaging_application_planning_residual STATIC");
    const std::string application_private_block = ExtractBlock(
        content,
        "target_link_libraries(siligen_dispense_packaging_application PRIVATE");
    const std::string planning_common_private_block = ExtractBlock(
        content,
        "target_link_libraries(siligen_dispense_packaging_application_planning_common_residual PRIVATE");
    const std::string planning_authority_private_block = ExtractBlock(
        content,
        "target_link_libraries(siligen_dispense_packaging_application_planning_authority_residual PRIVATE");
    const std::string planning_execution_private_block = ExtractBlock(
        content,
        "target_link_libraries(siligen_dispense_packaging_application_planning_execution_residual PRIVATE");
    const std::string planning_residual_private_block = ExtractBlock(
        content,
        "target_link_libraries(siligen_dispense_packaging_application_planning_residual PRIVATE");

    EXPECT_NE(content.find("add_library(siligen_dispense_packaging_application STATIC"), std::string::npos);
    EXPECT_NE(
        content.find("add_library(siligen_dispense_packaging_application_planning_common_residual STATIC"),
        std::string::npos);
    EXPECT_NE(
        content.find("add_library(siligen_dispense_packaging_application_planning_authority_residual STATIC"),
        std::string::npos);
    EXPECT_NE(
        content.find("add_library(siligen_dispense_packaging_application_planning_execution_residual STATIC"),
        std::string::npos);
    EXPECT_NE(
        content.find("add_library(siligen_dispense_packaging_application_planning_residual STATIC"),
        std::string::npos);
    EXPECT_NE(application_source_block.find("services/dispensing/PlanningAssemblyServices.cpp"), std::string::npos);
    EXPECT_NE(
        planning_common_source_block.find("services/dispensing/PlanningAssemblyResidualCommon.cpp"),
        std::string::npos);
    EXPECT_NE(
        planning_authority_source_block.find("services/dispensing/PlanningAssemblyAuthorityArtifacts.cpp"),
        std::string::npos);
    EXPECT_NE(
        planning_authority_source_block.find("services/dispensing/PreviewSnapshotResidualProcessPath.cpp"),
        std::string::npos);
    EXPECT_NE(
        planning_execution_source_block.find("services/dispensing/PlanningAssemblyExecutionArtifacts.cpp"),
        std::string::npos);
    EXPECT_NE(
        planning_execution_source_block.find("services/dispensing/PlanningAssemblyExecutionInterpolation.cpp"),
        std::string::npos);
    EXPECT_NE(
        planning_execution_source_block.find("services/dispensing/PlanningAssemblyExecutionBinding.cpp"),
        std::string::npos);
    EXPECT_NE(
        planning_execution_source_block.find("services/dispensing/PlanningAssemblyExecutionPackaging.cpp"),
        std::string::npos);
    EXPECT_NE(
        planning_residual_source_block.find("services/dispensing/PlanningAssemblyResidualFacade.cpp"),
        std::string::npos);
    EXPECT_NE(
        application_private_block.find("siligen_dispense_packaging_application_planning_residual"),
        std::string::npos);
    EXPECT_EQ(
        application_private_block.find("siligen_dispense_packaging_domain_dispensing_residual_headers"),
        std::string::npos);
    EXPECT_NE(
        planning_common_private_block.find("siligen_dispense_packaging_planning_owner_residual"),
        std::string::npos);
    EXPECT_NE(
        planning_common_private_block.find("siligen_dispense_packaging_domain_dispensing_planning_residual_headers"),
        std::string::npos);
    EXPECT_EQ(
        planning_common_private_block.find("siligen_dispense_packaging_planning_legacy_dxf_residual"),
        std::string::npos);
    EXPECT_NE(
        planning_authority_private_block.find("siligen_dispense_packaging_application_planning_common_residual"),
        std::string::npos);
    EXPECT_NE(
        planning_authority_private_block.find("siligen_dispense_packaging_planning_owner_residual"),
        std::string::npos);
    EXPECT_EQ(
        planning_authority_private_block.find("siligen_dispense_packaging_planning_legacy_dxf_residual"),
        std::string::npos);
    EXPECT_NE(
        planning_execution_private_block.find("siligen_dispense_packaging_application_planning_common_residual"),
        std::string::npos);
    EXPECT_NE(
        planning_execution_private_block.find("siligen_dispense_packaging_domain_dispensing_planning_residual_headers"),
        std::string::npos);
    EXPECT_EQ(
        planning_execution_private_block.find("siligen_dispense_packaging_planning_legacy_dxf_residual"),
        std::string::npos);
    EXPECT_NE(
        planning_execution_private_block.find("${SILIGEN_DISPENSE_PACKAGING_MOTION_PLANNING_APP_TARGET}"),
        std::string::npos);
    EXPECT_NE(
        planning_residual_private_block.find("siligen_dispense_packaging_application_planning_common_residual"),
        std::string::npos);
    EXPECT_NE(
        planning_residual_private_block.find("siligen_dispense_packaging_application_planning_authority_residual"),
        std::string::npos);
    EXPECT_NE(
        planning_residual_private_block.find("siligen_dispense_packaging_application_planning_execution_residual"),
        std::string::npos);
    EXPECT_EQ(
        application_private_block.find("siligen_dispense_packaging_planning_residual"),
        std::string::npos);
    EXPECT_EQ(
        planning_residual_private_block.find("siligen_dispense_packaging_planning_legacy_dxf_residual"),
        std::string::npos);
    EXPECT_EQ(
        application_private_block.find("siligen_dispense_packaging_domain_dispensing\n"),
        std::string::npos);
    EXPECT_EQ(
        application_private_block.find("${SILIGEN_DISPENSE_PACKAGING_MOTION_PLANNING_APP_TARGET}"),
        std::string::npos);
    EXPECT_EQ(
        planning_residual_private_block.find("siligen_dispense_packaging_domain_dispensing_planning_residual_headers"),
        std::string::npos);
}

TEST(DispensePackagingResidualAcceptanceTest, ApplicationPublicStopsExportingValveResidualAndMotionPlanningTargets) {
    const fs::path repo_root = RepoRoot();
    const std::string content = ReadTextFile(
        repo_root / "modules/dispense-packaging/application/CMakeLists.txt");

    const std::string application_source_block = ExtractBlock(
        content,
        "add_library(siligen_dispense_packaging_application STATIC");
    const std::string planning_residual_source_block = ExtractBlock(
        content,
        "add_library(siligen_dispense_packaging_application_planning_residual STATIC");
    const std::string header_block = ExtractBlock(
        content,
        "target_link_libraries(siligen_dispense_packaging_application_headers INTERFACE");
    const std::string application_private_block = ExtractBlock(
        content,
        "target_link_libraries(siligen_dispense_packaging_application PRIVATE");
    const std::string planning_residual_private_block = ExtractBlock(
        content,
        "target_link_libraries(siligen_dispense_packaging_application_planning_residual PRIVATE");
    const std::string public_block = ExtractBlock(
        content,
        "target_link_libraries(siligen_dispense_packaging_application_public INTERFACE");

    EXPECT_NE(application_source_block.find("services/dispensing/PlanningAssemblyServices.cpp"), std::string::npos);
    EXPECT_NE(
        planning_residual_source_block.find("services/dispensing/PlanningAssemblyResidualFacade.cpp"),
        std::string::npos);
    EXPECT_EQ(
        planning_residual_source_block.find("services/dispensing/PlanningAssemblyAuthorityArtifacts.cpp"),
        std::string::npos);
    EXPECT_EQ(header_block.find("siligen_motion_planning_application_public"), std::string::npos);
    EXPECT_EQ(header_block.find("SILIGEN_DISPENSE_PACKAGING_MOTION_PLANNING_APP_TARGET"), std::string::npos);
    EXPECT_NE(
        application_private_block.find("siligen_dispense_packaging_application_planning_residual"),
        std::string::npos);
    EXPECT_EQ(
        application_private_block.find("siligen_dispense_packaging_domain_dispensing_residual_headers"),
        std::string::npos);
    EXPECT_NE(
        planning_residual_private_block.find("siligen_dispense_packaging_application_planning_execution_residual"),
        std::string::npos);
    EXPECT_EQ(
        planning_residual_private_block.find("siligen_dispense_packaging_domain_dispensing_planning_residual_headers"),
        std::string::npos);
    EXPECT_EQ(
        planning_residual_private_block.find("siligen_dispense_packaging_domain_dispensing_execution_residual_headers"),
        std::string::npos);
    EXPECT_EQ(application_private_block.find("siligen_dispense_packaging_planning_residual"), std::string::npos);
    EXPECT_EQ(application_private_block.find("${SILIGEN_DISPENSE_PACKAGING_MOTION_PLANNING_APP_TARGET}"), std::string::npos);
    EXPECT_EQ(application_private_block.find("siligen_motion_planning_application_public"), std::string::npos);
    EXPECT_EQ(application_private_block.find("siligen_dispense_packaging_domain_dispensing\n"), std::string::npos);
    EXPECT_EQ(public_block.find("siligen_valve_core"), std::string::npos);
    EXPECT_EQ(public_block.find("siligen_dispense_packaging_execution_residual"), std::string::npos);
    EXPECT_EQ(public_block.find("siligen_dispense_packaging_planning_residual"), std::string::npos);
    EXPECT_EQ(
        public_block.find("$<LINK_ONLY:siligen_dispense_packaging_application_planning_residual>"),
        std::string::npos);
}

TEST(DispensePackagingResidualAcceptanceTest, PreviewSnapshotServiceDelegatesProcessPathConcreteToPlanningResidual) {
    const fs::path repo_root = RepoRoot();
    const std::string cmake_content = ReadTextFile(
        repo_root / "modules/dispense-packaging/application/CMakeLists.txt");
    const std::string planning_authority_source_block = ExtractBlock(
        cmake_content,
        "add_library(siligen_dispense_packaging_application_planning_authority_residual STATIC");
    const std::string preview_service = ReadTextFile(
        repo_root / "modules/dispense-packaging/application/services/dispensing/PreviewSnapshotService.cpp");
    const std::string preview_residual = ReadTextFile(
        repo_root / "modules/dispense-packaging/application/services/dispensing/PreviewSnapshotResidualProcessPath.cpp");

    EXPECT_NE(
        planning_authority_source_block.find("services/dispensing/PreviewSnapshotResidualProcessPath.cpp"),
        std::string::npos);
    EXPECT_NE(
        preview_service.find(
            '#' + std::string("include \"application/services/dispensing/PreviewSnapshotResidualProcessPath.h\"")),
        std::string::npos);
    EXPECT_NE(
        preview_service.find("Internal::BuildPreviewProcessPathPoints(*input.process_path)"),
        std::string::npos);
    EXPECT_EQ(
        preview_service.find(
            '#' + std::string("include \"domain/dispensing/planning/domain-services/CurveFlatteningService.h\"")),
        std::string::npos);
    EXPECT_EQ(
        preview_service.find("std::vector<Point2D> BuildPointVectorFromProcessPath("),
        std::string::npos);
    EXPECT_NE(
        preview_residual.find(
            '#' + std::string("include \"domain/dispensing/planning/domain-services/CurveFlatteningService.h\"")),
        std::string::npos);
    EXPECT_NE(preview_residual.find("BuildPreviewProcessPathPoints("), std::string::npos);
}

TEST(DispensePackagingResidualAcceptanceTest, PlanningResidualFacadeShrinksToOrchestrationOnly) {
    const fs::path repo_root = RepoRoot();
    const std::string content = ReadTextFile(
        repo_root / "modules/dispense-packaging/application/services/dispensing/PlanningAssemblyResidualFacade.cpp");

    EXPECT_EQ(CountOccurrences(content, "BuildAuthorityPreviewArtifactsResidual("), 1U);
    EXPECT_EQ(CountOccurrences(content, "BuildExecutionArtifactsFromAuthorityResidual("), 1U);
    EXPECT_NE(content.find("AssembleAuthorityPreviewArtifacts("), std::string::npos);
    EXPECT_NE(content.find("AssembleExecutionArtifacts("), std::string::npos);

    EXPECT_EQ(content.find("BuildTriggerArtifacts("), std::string::npos);
    EXPECT_EQ(content.find("BuildInterpolationPoints("), std::string::npos);
    EXPECT_EQ(content.find("BindAuthorityLayoutToExecutionTrajectory("), std::string::npos);

    EXPECT_EQ(content.find("CmpInterpolationFacade.h"), std::string::npos);
    EXPECT_EQ(content.find("TrajectoryInterpolationFacade.h"), std::string::npos);
    EXPECT_EQ(content.find("PlanningArtifactExportAssemblyService.h"), std::string::npos);
    EXPECT_EQ(content.find("InterpolationProgramPlanner.h"), std::string::npos);
    EXPECT_EQ(content.find("AuthorityTriggerLayoutPlanner.h"), std::string::npos);
    EXPECT_EQ(content.find("CurveFlatteningService.h"), std::string::npos);
    EXPECT_EQ(content.find("TriggerPlanner.h"), std::string::npos);
}

TEST(DispensePackagingResidualAcceptanceTest, ExecutionResidualSplitsIntoModuleLocalPrivateTranslationUnits) {
    const fs::path repo_root = RepoRoot();
    const std::string cmake_content = ReadTextFile(
        repo_root / "modules/dispense-packaging/application/CMakeLists.txt");
    const std::string planning_execution_source_block = ExtractBlock(
        cmake_content,
        "add_library(siligen_dispense_packaging_application_planning_execution_residual STATIC");
    const std::string execution_artifacts = ReadTextFile(
        repo_root / "modules/dispense-packaging/application/services/dispensing/PlanningAssemblyExecutionArtifacts.cpp");
    const std::string execution_interpolation = ReadTextFile(
        repo_root / "modules/dispense-packaging/application/services/dispensing/PlanningAssemblyExecutionInterpolation.cpp");
    const std::string execution_packaging = ReadTextFile(
        repo_root / "modules/dispense-packaging/application/services/dispensing/PlanningAssemblyExecutionPackaging.cpp");

    EXPECT_NE(
        planning_execution_source_block.find("services/dispensing/PlanningAssemblyExecutionInterpolation.cpp"),
        std::string::npos);
    EXPECT_NE(
        planning_execution_source_block.find("services/dispensing/PlanningAssemblyExecutionBinding.cpp"),
        std::string::npos);
    EXPECT_NE(
        planning_execution_source_block.find("services/dispensing/PlanningAssemblyExecutionPackaging.cpp"),
        std::string::npos);

    EXPECT_NE(execution_artifacts.find("AssembleExecutionArtifacts("), std::string::npos);
    EXPECT_EQ(
        execution_artifacts.find("Result<std::vector<TrajectoryPoint>> BuildInterpolationPoints("),
        std::string::npos);
    EXPECT_EQ(
        execution_artifacts.find("void BindAuthorityLayoutToExecutionTrajectory("),
        std::string::npos);
    EXPECT_EQ(
        execution_artifacts.find("TriggerArtifacts BuildTriggerArtifactsFromAuthorityPreview("),
        std::string::npos);
    EXPECT_EQ(
        execution_artifacts.find("Result<ExecutionPackageValidated> BuildValidatedExecutionPackage("),
        std::string::npos);
    EXPECT_EQ(
        execution_artifacts.find(
            "Siligen::Domain::Dispensing::Contracts::PlanningArtifactExportRequest BuildExecutionExportRequest("),
        std::string::npos);
    EXPECT_EQ(
        execution_artifacts.find("Result<ExecutionGenerationArtifacts> BuildExecutionGenerationArtifacts("),
        std::string::npos);
    EXPECT_EQ(execution_artifacts.find("TriggerBindingCandidate"), std::string::npos);
    EXPECT_EQ(execution_artifacts.find("TriggerBindingTraceRow"), std::string::npos);

    EXPECT_EQ(execution_artifacts.find("CmpInterpolationFacade.h"), std::string::npos);
    EXPECT_EQ(execution_artifacts.find("TrajectoryInterpolationFacade.h"), std::string::npos);
    EXPECT_EQ(execution_artifacts.find("InterpolationProgramPlanner.h"), std::string::npos);
    EXPECT_EQ(execution_artifacts.find("PlanningArtifactExportAssemblyService.h"), std::string::npos);

    EXPECT_NE(
        execution_interpolation.find("Result<ExecutionGenerationArtifacts> BuildExecutionGenerationArtifacts("),
        std::string::npos);
    EXPECT_NE(execution_interpolation.find("CmpInterpolationFacade.h"), std::string::npos);
    EXPECT_NE(execution_interpolation.find("TrajectoryInterpolationFacade.h"), std::string::npos);
    EXPECT_NE(execution_interpolation.find("InterpolationProgramPlanner.h"), std::string::npos);

    EXPECT_NE(
        execution_packaging.find("Result<ExecutionPackageValidated> BuildValidatedExecutionPackage("),
        std::string::npos);
    EXPECT_EQ(execution_packaging.find("InterpolationProgramPlanner.h"), std::string::npos);
    EXPECT_NE(execution_packaging.find("PlanningArtifactExportAssemblyService.h"), std::string::npos);
}

TEST(DispensePackagingResidualAcceptanceTest, LegacyPlannerLiveConsumerMovesToResidualQuarantineSupportAndAudit) {
    const fs::path repo_root = RepoRoot();
    const std::string workflow_integration = ReadTextFile(
        repo_root / "modules/workflow/tests/integration/DispensingWorkflowUseCaseTest.cpp");
    const std::string motion_planning_tests = ReadTextFile(
        repo_root / "modules/motion-planning/tests/CMakeLists.txt");
    const std::string dispense_packaging_tests = ReadTextFile(
        repo_root / "modules/dispense-packaging/tests/CMakeLists.txt");
    const std::string dispense_packaging_domain = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/CMakeLists.txt");

    EXPECT_EQ(
        workflow_integration.find('#' + std::string("include \"domain/dispensing/planning/domain-services/DispensingPlannerService.h\"")),
        std::string::npos);
    EXPECT_EQ(workflow_integration.find("DomainServices::DispensingPlan"), std::string::npos);
    EXPECT_NE(
        motion_planning_tests.find("unit/domain/motion/MainlineTrajectoryAuditTest.cpp"),
        std::string::npos);
    EXPECT_EQ(
        motion_planning_tests.find("siligen_dispense_packaging_planning_legacy_dxf_residual"),
        std::string::npos);
    EXPECT_EQ(
        motion_planning_tests.find("siligen_dispense_packaging_application_public"),
        std::string::npos);
    EXPECT_EQ(
        motion_planning_tests.find("siligen_dispense_packaging_domain_dispensing"),
        std::string::npos);
    EXPECT_EQ(
        dispense_packaging_domain.find("siligen_dispense_packaging_planning_legacy_dxf_residual"),
        std::string::npos);
    EXPECT_EQ(
        dispense_packaging_domain.find("siligen_dispense_packaging_planning_residual"),
        std::string::npos);
    EXPECT_NE(
        dispense_packaging_tests.find("siligen_dispense_packaging_planning_legacy_dxf_quarantine_support STATIC"),
        std::string::npos);
    EXPECT_NE(
        dispense_packaging_tests.find("unit/domain/dispensing/LegacyDxfPlannerQuarantineAuditTest.cpp"),
        std::string::npos);
    EXPECT_NE(
        dispense_packaging_tests.find("siligen_dispense_packaging_planning_legacy_dxf_quarantine_support"),
        std::string::npos);
}

}  // namespace
