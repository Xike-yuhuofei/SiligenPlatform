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

TEST(DispensePackagingBoundaryTest, PublicAssemblyServicesUseWorkflowTypesInsteadOfStageTypes) {
    const fs::path repo_root = RepoRoot();
    const std::string authority_header = ReadTextFile(
        repo_root / "modules/dispense-packaging/application/include/application/services/dispensing/AuthorityPreviewAssemblyService.h");
    const std::string execution_header = ReadTextFile(
        repo_root / "modules/dispense-packaging/application/include/application/services/dispensing/ExecutionAssemblyService.h");

    EXPECT_NE(
        authority_header.find(
            '#' + std::string("include \"application/services/dispensing/WorkflowPlanningAssemblyTypes.h\"")),
        std::string::npos);
    EXPECT_NE(
        execution_header.find(
            '#' + std::string("include \"application/services/dispensing/WorkflowPlanningAssemblyTypes.h\"")),
        std::string::npos);
    EXPECT_EQ(
        authority_header.find('#' + std::string("include \"application/services/dispensing/PlanningAssemblyTypes.h\"")),
        std::string::npos);
    EXPECT_EQ(
        execution_header.find('#' + std::string("include \"application/services/dispensing/PlanningAssemblyTypes.h\"")),
        std::string::npos);
}

TEST(DispensePackagingBoundaryTest, DomainDispensingStopsExportingWorkflowAndRuntimeRawIncludeRoots) {
    const fs::path repo_root = RepoRoot();
    const std::string content = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/CMakeLists.txt");

    const std::string include_block = ExtractBlock(
        content,
        "target_include_directories(siligen_dispense_packaging_domain_dispensing BEFORE INTERFACE");
    const std::string link_block = ExtractBlock(
        content,
        "target_link_libraries(siligen_dispense_packaging_domain_dispensing INTERFACE");

    EXPECT_EQ(include_block.find("SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR"), std::string::npos);
    EXPECT_EQ(include_block.find("SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR"), std::string::npos);
    EXPECT_NE(link_block.find("siligen_runtime_execution_runtime_contracts"), std::string::npos);
    EXPECT_EQ(link_block.find("SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR"), std::string::npos);
    EXPECT_EQ(link_block.find("SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR"), std::string::npos);
    EXPECT_EQ(link_block.find("siligen_workflow_domain_public"), std::string::npos);
    EXPECT_EQ(link_block.find("siligen_workflow_domain_headers"), std::string::npos);
    EXPECT_EQ(content.find("GuardDecision bridge headers"), std::string::npos);
}

TEST(DispensePackagingBoundaryTest, ModuleRootRequiresApplicationPublicTargetAndRefusesDomainFallback) {
    const fs::path repo_root = RepoRoot();
    const std::string content = ReadTextFile(
        repo_root / "modules/dispense-packaging/CMakeLists.txt");

    EXPECT_NE(content.find("refusing to fall back to siligen_dispense_packaging_domain_dispensing"), std::string::npos);
    EXPECT_EQ(content.find("siligen_dispense_packaging_domain_dispensing\n    )"), std::string::npos);
}

TEST(DispensePackagingBoundaryTest, RuntimeServiceComposesM9DispensingProcessPortFactoryInsteadOfM8ExecutionWrapper) {
    const fs::path repo_root = RepoRoot();
    const std::string content = ReadTextFile(
        repo_root / "apps/runtime-service/container/ApplicationContainer.Dispensing.cpp");

    EXPECT_NE(
        content.find(
            '#' +
            std::string("include \"runtime_execution/application/services/dispensing/DispensingProcessPortFactory.h\"")),
        std::string::npos);
    EXPECT_NE(content.find("CreateDispensingProcessPort("), std::string::npos);
    EXPECT_EQ(
        content.find('#' + std::string("include \"runtime/dispensing/WorkflowDispensingProcessPortAdapter.h\"")),
        std::string::npos);
    EXPECT_EQ(content.find("WorkflowDispensingProcessPortAdapter"), std::string::npos);
}

TEST(DispensePackagingBoundaryTest, LegacyExecutionProviderAndTestsAreRemovedFromM8AndWorkflow) {
    const fs::path repo_root = RepoRoot();

    EXPECT_FALSE(fs::exists(
        repo_root / "modules/dispense-packaging/application/include/application/services/dispensing/WorkflowDispensingProcessOperations.h"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/dispense-packaging/application/include/application/services/dispensing/WorkflowDispensingProcessOperationsProvider.h"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/dispense-packaging/application/services/dispensing/WorkflowDispensingProcessOperationsProvider.cpp"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/dispense-packaging/tests/unit/application/services/dispensing/WorkflowDispensingProcessOperationsProviderTest.cpp"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/dispense-packaging/tests/unit/domain/dispensing/DispensingProcessServiceTraceTest.cpp"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/tests/process-runtime-core/unit/dispensing/DispensingProcessServiceWaitForMotionCompleteTest.cpp"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/domain/domain/dispensing/domain-services/DispensingProcessService.h"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/domain/domain/dispensing/domain-services/DispensingProcessService.cpp"));

    EXPECT_TRUE(fs::exists(repo_root / "modules/runtime-execution/tests/unit/DispensingProcessPortFactoryTest.cpp"));
    EXPECT_TRUE(fs::exists(repo_root / "modules/runtime-execution/tests/unit/DispensingProcessServiceTraceTest.cpp"));
    EXPECT_TRUE(fs::exists(repo_root / "modules/runtime-execution/tests/unit/DispensingProcessServiceWaitForMotionCompleteTest.cpp"));
}

TEST(DispensePackagingBoundaryTest, PlanningUseCaseAndPortsAreOwnedByDispensePackagingApplication) {
    const fs::path repo_root = RepoRoot();
    const std::string canonical_header = ReadTextFile(
        repo_root / "modules/dispense-packaging/application/include/dispense_packaging/application/usecases/dispensing/PlanningUseCase.h");
    const std::string ports_header = ReadTextFile(
        repo_root / "modules/dispense-packaging/application/include/dispense_packaging/application/usecases/dispensing/PlanningPorts.h");
    const std::string adapters_header = ReadTextFile(
        repo_root / "modules/dispense-packaging/application/include/dispense_packaging/application/usecases/dispensing/PlanningPortAdapters.h");
    const std::string source = ReadTextFile(
        repo_root / "modules/dispense-packaging/application/usecases/dispensing/PlanningUseCase.cpp");

    EXPECT_NE(
        canonical_header.find("Result<PreparedAuthorityPreview> PrepareAuthorityPreview("),
        std::string::npos);
    EXPECT_NE(
        canonical_header.find("Result<ExecutionAssemblyResponse> AssembleExecutionFromAuthority("),
        std::string::npos);
    EXPECT_EQ(
        canonical_header.find("PlanningUseCaseInternal"),
        std::string::npos);
    EXPECT_EQ(
        source.find("PlanningUseCaseInternal"),
        std::string::npos);
    EXPECT_NE(
        source.find(
            '#' +
            std::string("include \"application/services/dispensing/WorkflowPlanningAssemblyOperations.h\"")),
        std::string::npos);
    EXPECT_NE(
        ports_header.find("class IProcessPathBuildPort"),
        std::string::npos);
    EXPECT_NE(
        adapters_header.find("AdaptProcessPathFacade("),
        std::string::npos);
    EXPECT_NE(
        adapters_header.find("AdaptMotionPlanningFacade("),
        std::string::npos);
    EXPECT_NE(
        adapters_header.find("AdaptDxfPreparationService("),
        std::string::npos);
    EXPECT_EQ(
        source.find('#' + std::string("include \"application/services/dispensing/WorkflowPlanningAssemblyOperationsProvider.h\"")),
        std::string::npos);
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/application/include/application/planning-trigger/PlanningUseCase.h"));
    EXPECT_FALSE(fs::exists(repo_root / "modules/workflow/application/planning-trigger/PlanningUseCase.h"));
    EXPECT_FALSE(fs::exists(repo_root / "modules/workflow/application/planning-trigger/PlanningUseCase.cpp"));
    EXPECT_FALSE(fs::exists(repo_root / "modules/workflow/application/planning-trigger/PlanningUseCaseInternal.h"));
    EXPECT_FALSE(fs::exists(repo_root / "modules/workflow/application/ports/dispensing/PlanningPorts.h"));
    EXPECT_FALSE(fs::exists(repo_root / "modules/workflow/application/ports/dispensing/PlanningPortAdapters.h"));
}

TEST(DispensePackagingBoundaryTest, WorkflowPlanningAssemblyPublicSeamUsesWorkflowTypesInsteadOfStageTypes) {
    const fs::path repo_root = RepoRoot();
    const std::string content = ReadTextFile(
        repo_root / "modules/dispense-packaging/application/include/application/services/dispensing/WorkflowPlanningAssemblyOperations.h");

    EXPECT_NE(
        content.find(
            '#' +
            std::string("include \"application/services/dispensing/WorkflowPlanningAssemblyTypes.h\"")),
        std::string::npos);
    EXPECT_EQ(
        content.find('#' + std::string("include \"application/services/dispensing/PlanningAssemblyTypes.h\"")),
        std::string::npos);
}

TEST(DispensePackagingBoundaryTest, PlanningAssemblyStageTypesHeaderIsRemovedFromPublicIncludeRoot) {
    const fs::path repo_root = RepoRoot();
    const fs::path public_header =
        repo_root / "modules/dispense-packaging/application/include/application/services/dispensing/PlanningAssemblyTypes.h";

    EXPECT_FALSE(fs::exists(public_header));
}

TEST(DispensePackagingBoundaryTest, PlanningArtifactExportPortLivesInDispensePackagingApplicationSurface) {
    const fs::path repo_root = RepoRoot();
    const fs::path owner_header =
        repo_root / "modules/dispense-packaging/application/include/application/services/dispensing/PlanningArtifactExportPort.h";
    const std::string owner_content = ReadTextFile(owner_header);

    EXPECT_TRUE(fs::exists(owner_header));
    EXPECT_NE(
        owner_content.find('#' + std::string("include \"dispense_packaging/contracts/PlanningArtifactExportRequest.h\"")),
        std::string::npos);
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/runtime-execution/application/include/runtime_execution/application/services/dispensing/PlanningArtifactExportPort.h"));
}

TEST(DispensePackagingBoundaryTest, PlanningArtifactExportRequestRemainsContractOwnedWithoutAppAlias) {
    const fs::path repo_root = RepoRoot();
    const std::string contract_header = ReadTextFile(
        repo_root / "modules/dispense-packaging/contracts/include/domain/dispensing/contracts/PlanningArtifactExportRequest.h");
    const std::string owner_port_header = ReadTextFile(
        repo_root / "modules/dispense-packaging/application/include/application/services/dispensing/PlanningArtifactExportPort.h");

    EXPECT_EQ(contract_header.find("namespace Siligen::Application::Services::Dispensing"), std::string::npos);
    EXPECT_EQ(contract_header.find("using PlanningArtifactExportRequest ="), std::string::npos);
    EXPECT_NE(
        owner_port_header.find("Siligen::Domain::Dispensing::Contracts::PlanningArtifactExportRequest"),
        std::string::npos);
    EXPECT_EQ(owner_port_header.find("const PlanningArtifactExportRequest& request"), std::string::npos);
}

TEST(DispensePackagingBoundaryTest, LocalWorkflowPlanningTypesHeaderOwnsCanonicalWorkflowDtoDefinitions) {
    const fs::path repo_root = RepoRoot();
    const std::string content = ReadTextFile(
        repo_root / "modules/dispense-packaging/application/include/application/services/dispensing/WorkflowPlanningAssemblyTypes.h");

    EXPECT_EQ(content.find("compat forwarder"), std::string::npos);
    EXPECT_EQ(
        content.find(
            '#' +
            std::string(
                "include \"../../../../../../workflow/application/include/workflow/application/services/dispensing/WorkflowPlanningAssemblyTypes.h\"")),
        std::string::npos);
    EXPECT_NE(content.find("struct WorkflowAuthorityTriggerPoint"), std::string::npos);
    EXPECT_NE(content.find("struct WorkflowPlanningAssemblyResult"), std::string::npos);
}

TEST(DispensePackagingBoundaryTest, WorkflowPlanningTypesLegacyCompatHeadersAreRemovedFromWorkflow) {
    const fs::path repo_root = RepoRoot();
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/application/services/dispensing/WorkflowPlanningAssemblyTypes.h"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/application/include/application/services/dispensing/WorkflowPlanningAssemblyTypes.h"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/application/include/workflow/application/services/dispensing/WorkflowPlanningAssemblyTypes.h"));
}

TEST(DispensePackagingBoundaryTest, ForeignPlanningAndExecutionTestsAreHostedByOwnerModules) {
    const fs::path repo_root = RepoRoot();

    EXPECT_TRUE(fs::exists(
        repo_root / "modules/dispense-packaging/tests/unit/application/usecases/dispensing/PlanningRequestTest.cpp"));
    EXPECT_TRUE(fs::exists(
        repo_root / "modules/dispense-packaging/tests/unit/application/usecases/dispensing/PlanningUseCaseExportPortTest.cpp"));
    EXPECT_TRUE(fs::exists(
        repo_root / "modules/dispense-packaging/tests/unit/application/usecases/dispensing/PlanningFailureSurfaceTest.cpp"));
    EXPECT_TRUE(fs::exists(
        repo_root / "modules/runtime-execution/tests/unit/dispensing/DispensingWorkflowUseCaseTest.cpp"));

    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/tests/canonical/unit/dispensing/PlanningRequestTest.cpp"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/tests/integration/PlanningUseCaseExportPortTest.cpp"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/tests/integration/PlanningFailureSurfaceTest.cpp"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/tests/integration/DispensingWorkflowUseCaseTest.cpp"));
}

TEST(DispensePackagingBoundaryTest, ExecutionPackageUsesPlanOnlyBackingTypeAndRuntimeContractsOwnRuntimeTypes) {
    const fs::path repo_root = RepoRoot();
    const std::string package_header = ReadTextFile(
        repo_root / "modules/dispense-packaging/contracts/include/domain/dispensing/contracts/ExecutionPackage.h");
    const std::string runtime_types = ReadTextFile(
        repo_root / "modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/dispensing/DispensingExecutionTypes.h");
    const std::string compat_header = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/value-objects/DispensingExecutionTypes.h");

    EXPECT_NE(
        package_header.find('#' + std::string("include \"domain/dispensing/value-objects/DispensingExecutionPlan.h\"")),
        std::string::npos);
    EXPECT_EQ(
        package_header.find('#' + std::string("include \"domain/dispensing/value-objects/DispensingExecutionTypes.h\"")),
        std::string::npos);
    EXPECT_NE(runtime_types.find("struct DispensingRuntimeOverrides"), std::string::npos);
    EXPECT_NE(runtime_types.find("struct DispensingExecutionOptions"), std::string::npos);
    EXPECT_EQ(runtime_types.find("struct DispensingExecutionPlan"), std::string::npos);
    EXPECT_NE(
        compat_header.find(
            '#' + std::string("include \"runtime_execution/contracts/dispensing/DispensingExecutionTypes.h\"")),
        std::string::npos);
}

TEST(DispensePackagingBoundaryTest, LegacyDispensingPortsAndDtosLiveOnlyOnOwnerModules) {
    const fs::path repo_root = RepoRoot();
    const std::string runtime_compensation = ReadTextFile(
        repo_root / "modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/dispensing/DispenseCompensationProfile.h");
    const std::string runtime_quality = ReadTextFile(
        repo_root / "modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/dispensing/QualityMetrics.h");
    const std::string runtime_valve = ReadTextFile(
        repo_root / "modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/dispensing/IValvePort.h");
    const std::string runtime_trigger = ReadTextFile(
        repo_root / "modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/dispensing/ITriggerControllerPort.h");
    const std::string runtime_scheduler = ReadTextFile(
        repo_root / "modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/dispensing/ITaskSchedulerPort.h");
    const std::string runtime_observer = ReadTextFile(
        repo_root / "modules/runtime-execution/contracts/runtime/include/runtime_execution/contracts/dispensing/IDispensingExecutionObserver.h");

    const std::string package_compensation = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/value-objects/DispenseCompensationProfile.h");
    const std::string package_quality = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/value-objects/QualityMetrics.h");
    const std::string package_valve = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/ports/IValvePort.h");
    const std::string package_trigger = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/ports/ITriggerControllerPort.h");
    const std::string package_scheduler = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/ports/ITaskSchedulerPort.h");
    const std::string package_observer = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/ports/IDispensingExecutionObserver.h");

    EXPECT_NE(runtime_compensation.find("struct DispenseCompensationProfile"), std::string::npos);
    EXPECT_NE(runtime_quality.find("struct QualityMetrics"), std::string::npos);
    EXPECT_NE(runtime_valve.find("class IValvePort"), std::string::npos);
    EXPECT_NE(runtime_trigger.find("class ITriggerControllerPort"), std::string::npos);
    EXPECT_NE(runtime_scheduler.find("class ITaskSchedulerPort"), std::string::npos);
    EXPECT_NE(runtime_observer.find("class IDispensingExecutionObserver"), std::string::npos);

    EXPECT_NE(package_compensation.find("runtime_execution/contracts"), std::string::npos);
    EXPECT_NE(package_quality.find("runtime_execution/contracts"), std::string::npos);
    EXPECT_NE(package_valve.find("runtime_execution/contracts"), std::string::npos);
    EXPECT_NE(package_trigger.find("runtime_execution/contracts"), std::string::npos);
    EXPECT_NE(package_scheduler.find("runtime_execution/contracts"), std::string::npos);
    EXPECT_NE(package_observer.find("runtime_execution/contracts"), std::string::npos);
    EXPECT_EQ(package_compensation.find("struct DispenseCompensationProfile"), std::string::npos);
    EXPECT_EQ(package_quality.find("struct QualityMetrics"), std::string::npos);
    EXPECT_EQ(package_valve.find("class IValvePort"), std::string::npos);
    EXPECT_EQ(package_trigger.find("class ITriggerControllerPort"), std::string::npos);
    EXPECT_EQ(package_scheduler.find("class ITaskSchedulerPort"), std::string::npos);
    EXPECT_EQ(package_observer.find("class IDispensingExecutionObserver"), std::string::npos);

    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/domain/include/domain/dispensing/value-objects/DispenseCompensationProfile.h"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/domain/include/domain/dispensing/value-objects/QualityMetrics.h"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/domain/include/domain/dispensing/ports/IValvePort.h"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/domain/include/domain/dispensing/ports/ITriggerControllerPort.h"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/domain/include/domain/dispensing/ports/ITaskSchedulerPort.h"));
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/domain/domain/dispensing/ports/IDispensingExecutionObserver.h"));
}

TEST(DispensePackagingBoundaryTest, ApplicationPublicStopsExportingValveResidualAndMotionPlanningTargets) {
    const fs::path repo_root = RepoRoot();
    const std::string content = ReadTextFile(
        repo_root / "modules/dispense-packaging/application/CMakeLists.txt");

    const std::string header_block = ExtractBlock(
        content,
        "target_link_libraries(siligen_dispense_packaging_application_headers INTERFACE");
    const std::string public_block = ExtractBlock(
        content,
        "target_link_libraries(siligen_dispense_packaging_application_public INTERFACE");

    EXPECT_EQ(header_block.find("siligen_motion_planning_application_public"), std::string::npos);
    EXPECT_EQ(header_block.find("SILIGEN_DISPENSE_PACKAGING_MOTION_PLANNING_APP_TARGET"), std::string::npos);
    EXPECT_EQ(public_block.find("siligen_valve_core"), std::string::npos);
    EXPECT_EQ(public_block.find("siligen_dispense_packaging_execution_residual"), std::string::npos);
    EXPECT_EQ(public_block.find("siligen_dispense_packaging_planning_residual"), std::string::npos);
}

TEST(DispensePackagingBoundaryTest, WorkflowPlanningShimHeaderIsRemoved) {
    const fs::path repo_root = RepoRoot();
    const fs::path shim =
        repo_root / "modules/workflow/application/include/application/services/dispensing/PlanningAssemblyTypes.h";

    EXPECT_FALSE(fs::exists(shim));
}

TEST(DispensePackagingBoundaryTest, WorkflowPlanningPreviewAssemblyResidualIsRemoved) {
    const fs::path repo_root = RepoRoot();
    const fs::path header =
        repo_root / "modules/workflow/application/services/dispensing/PlanningPreviewAssemblyService.h";
    const fs::path source =
        repo_root / "modules/workflow/application/services/dispensing/PlanningPreviewAssemblyService.cpp";

    EXPECT_FALSE(fs::exists(header));
    EXPECT_FALSE(fs::exists(source));
}

TEST(DispensePackagingBoundaryTest, WorkflowLegacyTriggerPlanHeaderIsDeleted) {
    const fs::path repo_root = RepoRoot();
    EXPECT_FALSE(fs::exists(
        repo_root / "modules/workflow/domain/domain/dispensing/value-objects/TriggerPlan.h"));
}

}  // namespace
