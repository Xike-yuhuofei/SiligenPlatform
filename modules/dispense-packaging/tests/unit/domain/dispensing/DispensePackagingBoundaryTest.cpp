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
    const std::string content = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/planning/domain-services/UnifiedTrajectoryPlannerService.cpp");

    EXPECT_NE(content.find('#' + std::string("include \"application/services/process_path/ProcessPathFacade.h\"")),
              std::string::npos);
    EXPECT_EQ(content.find('#' + std::string("include \"domain-services/GeometryNormalizer.h\"")), std::string::npos);
    EXPECT_EQ(content.find('#' + std::string("include \"domain-services/ProcessAnnotator.h\"")), std::string::npos);
    EXPECT_EQ(content.find('#' + std::string("include \"domain-services/TrajectoryShaper.h\"")), std::string::npos);
}

TEST(DispensePackagingBoundaryTest, DispensePackagingTargetLinksProcessPathApplicationPublicOnly) {
    const fs::path repo_root = RepoRoot();
    const std::string content = ReadTextFile(
        repo_root / "modules/dispense-packaging/domain/dispensing/CMakeLists.txt");

    EXPECT_NE(content.find("siligen_process_path_application_public"), std::string::npos);
    EXPECT_EQ(content.find("siligen_process_path_domain_trajectory"), std::string::npos);
}

}  // namespace
