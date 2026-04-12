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

TEST(DispensePackagingResidualAcceptanceTest, UnifiedTrajectoryPlannerUsesProcessPathFacadeInsteadOfOwnerHeaders) {
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

TEST(DispensePackagingResidualAcceptanceTest, DispensePackagingOwnerTargetShrinksToCoreAndResidualTargetsCarryConcreteLinks) {
    const fs::path repo_root = RepoRoot();
    const std::string content = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/CMakeLists.txt");
    const std::string domain_link_block = ExtractBlock(
        content,
        "target_link_libraries(siligen_dispense_packaging_domain_dispensing INTERFACE");
    const std::string planning_link_block = ExtractBlock(
        content,
        "target_link_libraries(siligen_dispense_packaging_planning_residual PUBLIC");

    EXPECT_NE(content.find("add_library(siligen_dispense_packaging_domain_dispensing INTERFACE)"), std::string::npos);
    EXPECT_NE(content.find("add_library(siligen_dispense_packaging_planning_residual STATIC"), std::string::npos);
    EXPECT_NE(content.find("add_library(siligen_dispense_packaging_execution_residual STATIC"), std::string::npos);
    EXPECT_NE(planning_link_block.find("siligen_process_path_application_public"), std::string::npos);
    EXPECT_NE(planning_link_block.find("${SILIGEN_DISPENSE_PACKAGING_MOTION_PLANNING_APP_TARGET}"), std::string::npos);
    EXPECT_EQ(content.find("siligen_process_path_domain_trajectory"), std::string::npos);
    EXPECT_EQ(content.find("target_link_libraries(siligen_dispense_packaging_domain_dispensing PUBLIC"), std::string::npos);
    EXPECT_EQ(domain_link_block.find("siligen_process_path_application_public"), std::string::npos);
    EXPECT_EQ(domain_link_block.find("siligen_motion_planning_application_public"), std::string::npos);
    EXPECT_EQ(domain_link_block.find("${SILIGEN_DISPENSE_PACKAGING_MOTION_PLANNING_APP_TARGET}"), std::string::npos);
}

}  // namespace
