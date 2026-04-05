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

TEST(DispensePackagingBoundaryTest, UnifiedTrajectoryPlannerUsesProcessPathFacadeInsteadOfOwnerHeaders) {
    const fs::path repo_root = RepoRoot();
    const std::string header = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/planning/domain-services/UnifiedTrajectoryPlannerService.h");
    const std::string source = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/planning/domain-services/UnifiedTrajectoryPlannerService.cpp");

    EXPECT_NE(header.find('#' + std::string("include \"application/services/process_path/ProcessPathFacade.h\"")),
              std::string::npos);
    EXPECT_NE(source.find("process_path_facade_.Build"), std::string::npos);
    EXPECT_EQ(source.find('#' + std::string("include \"domain-services/GeometryNormalizer.h\"")), std::string::npos);
    EXPECT_EQ(source.find('#' + std::string("include \"domain-services/ProcessAnnotator.h\"")), std::string::npos);
    EXPECT_EQ(source.find('#' + std::string("include \"domain-services/TrajectoryShaper.h\"")), std::string::npos);
}

TEST(DispensePackagingBoundaryTest, DispensePackagingTargetLinksProcessPathApplicationPublicOnly) {
    const fs::path repo_root = RepoRoot();
    const std::string content = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/CMakeLists.txt");

    EXPECT_NE(content.find("siligen_process_path_application_public"), std::string::npos);
    EXPECT_EQ(content.find("siligen_process_path_domain_trajectory"), std::string::npos);
}

TEST(DispensePackagingBoundaryTest, ModuleRootRequiresApplicationPublicTargetAndRefusesDomainFallback) {
    const fs::path repo_root = RepoRoot();
    const std::string content = ReadTextFile(
        repo_root / "modules/dispense-packaging/CMakeLists.txt");

    EXPECT_NE(content.find("refusing to fall back to siligen_dispense_packaging_domain_dispensing"), std::string::npos);
    EXPECT_EQ(content.find("siligen_dispense_packaging_domain_dispensing\n    )"), std::string::npos);
}

TEST(DispensePackagingBoundaryTest, RuntimeServiceUsesApplicationProviderInsteadOfDirectDomainServiceInclude) {
    const fs::path repo_root = RepoRoot();
    const std::string content = ReadTextFile(
        repo_root / "apps/runtime-service/runtime/dispensing/WorkflowDispensingProcessOperations.cpp");

    EXPECT_NE(
        content.find('#' + std::string("include \"runtime/dispensing/WorkflowDispensingProcessOperations.h\"")),
        std::string::npos);
    EXPECT_EQ(
        content.find(
            '#' +
            std::string("include \"modules/dispense-packaging/domain/dispensing/domain-services/DispensingProcessService.h\"")),
        std::string::npos);
    EXPECT_NE(content.find("WorkflowDispensingProcessOperationsProvider{}.CreateOperations("), std::string::npos);
}

TEST(DispensePackagingBoundaryTest, WorkflowPlanningUseCaseConsumesSingleM8AssemblyProvider) {
    const fs::path repo_root = RepoRoot();
    const std::string content = ReadTextFile(
        repo_root / "modules/workflow/application/include/application/usecases/dispensing/PlanningUseCase.h");

    EXPECT_NE(
        content.find(
            '#' +
            std::string("include \"application/services/dispensing/WorkflowPlanningAssemblyOperationsProvider.h\"")),
        std::string::npos);
    EXPECT_EQ(
        content.find('#' + std::string("include \"application/services/dispensing/AuthorityPreviewAssemblyService.h\"")),
        std::string::npos);
    EXPECT_EQ(
        content.find('#' + std::string("include \"application/services/dispensing/ExecutionAssemblyService.h\"")),
        std::string::npos);
    EXPECT_EQ(
        content.find('#' + std::string("include \"application/services/dispensing/PlanningAssemblyTypes.h\"")),
        std::string::npos);
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

TEST(DispensePackagingBoundaryTest, WorkflowLegacyTriggerPlanHeaderForwardsToM8OwnerDefinition) {
    const fs::path repo_root = RepoRoot();
    const std::string content = ReadTextFile(
        repo_root / "modules/workflow/domain/domain/dispensing/value-objects/TriggerPlan.h");

    EXPECT_NE(content.find("Canonical owner lives in dispense-packaging domain."), std::string::npos);
    EXPECT_NE(
        content.find(
            '#' +
            std::string("include \"../../../../../dispense-packaging/domain/dispensing/value-objects/TriggerPlan.h\"")),
        std::string::npos);
    EXPECT_EQ(content.find("struct TriggerPlan"), std::string::npos);
}

}  // namespace
