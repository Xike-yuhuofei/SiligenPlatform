#include "runtime/configuration/WorkspaceAssetPaths.h"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace {

using Siligen::Infrastructure::Configuration::CanonicalizeOrKeep;
using Siligen::Infrastructure::Configuration::ConfigPathBridgeMode;
using Siligen::Infrastructure::Configuration::FindWorkspaceRoot;
using Siligen::Infrastructure::Configuration::ResolveConfigFilePath;
using Siligen::Infrastructure::Configuration::ResolveConfigFilePathWithBridge;

class ScopedCurrentPath {
public:
    explicit ScopedCurrentPath(const std::filesystem::path& target)
        : previous_(std::filesystem::current_path()) {
        std::filesystem::current_path(target);
    }

    ~ScopedCurrentPath() {
        std::error_code ec;
        std::filesystem::current_path(previous_, ec);
    }

private:
    std::filesystem::path previous_;
};

class ScopedTempWorkspace {
public:
    explicit ScopedTempWorkspace(const std::string& suffix) {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root_ = std::filesystem::temp_directory_path() /
                ("siligen_workspace_asset_paths_" + suffix + "_" + std::to_string(stamp));

        std::error_code ec;
        std::filesystem::create_directories(root_ / "cmake", ec);
        WriteFile("WORKSPACE.md", "# temp workspace\n");
        WriteFile("AGENTS.md", "# temp agents\n");
        WriteFile("cmake/workspace-layout.env", "SILIGEN_APPS_ROOT=apps\n");
    }

    ~ScopedTempWorkspace() {
        std::error_code ec;
        std::filesystem::remove_all(root_, ec);
    }

    [[nodiscard]] const std::filesystem::path& root() const noexcept {
        return root_;
    }

    std::filesystem::path CreateDirectory(const std::filesystem::path& relative_path) const {
        const auto absolute_path = root_ / relative_path;
        std::error_code ec;
        std::filesystem::create_directories(absolute_path, ec);
        return absolute_path;
    }

    std::filesystem::path WriteFile(
        const std::filesystem::path& relative_path,
        const std::string& content) const {
        const auto absolute_path = root_ / relative_path;
        std::error_code ec;
        std::filesystem::create_directories(absolute_path.parent_path(), ec);
        std::ofstream out(absolute_path, std::ios::binary);
        out << content;
        out.close();
        return absolute_path;
    }

private:
    std::filesystem::path root_;
};

bool HasWorkspaceMarkers(const std::filesystem::path& candidate) {
    static constexpr std::array<const char*, 3> kRequiredMarkers = {
        "WORKSPACE.md",
        "AGENTS.md",
        "cmake/workspace-layout.env",
    };

    std::error_code ec;
    for (const auto* marker : kRequiredMarkers) {
        if (!std::filesystem::exists(candidate / marker, ec) || ec) {
            return false;
        }
        ec.clear();
    }
    return true;
}

std::filesystem::path LocateWorkspaceRootFromSourcePath() {
    std::filesystem::path candidate = CanonicalizeOrKeep(std::filesystem::path(__FILE__)).parent_path();
    while (!candidate.empty()) {
        if (HasWorkspaceMarkers(candidate)) {
            return candidate;
        }
        const auto parent = candidate.parent_path();
        if (parent == candidate) {
            break;
        }
        candidate = parent;
    }
    return {};
}

}  // namespace

TEST(WorkspaceAssetPathsTest, FindsWorkspaceRootWhenAllMarkersExist) {
    const auto workspace_root = LocateWorkspaceRootFromSourcePath();
    ASSERT_FALSE(workspace_root.empty());

    ScopedCurrentPath scoped_cwd(workspace_root);
    const auto found_root = FindWorkspaceRoot();

    EXPECT_EQ(CanonicalizeOrKeep(found_root), CanonicalizeOrKeep(workspace_root));
}

TEST(WorkspaceAssetPathsTest, DoesNotTreatModuleCMakeListsAsWorkspaceRoot) {
    const auto workspace_root = LocateWorkspaceRootFromSourcePath();
    ASSERT_FALSE(workspace_root.empty());
    const auto runtime_host_root = workspace_root / "modules" / "runtime-execution" / "runtime" / "host";
    ASSERT_TRUE(std::filesystem::exists(runtime_host_root / "CMakeLists.txt"));

    ScopedCurrentPath scoped_cwd(runtime_host_root);
    const auto found_root = FindWorkspaceRoot();

    EXPECT_EQ(CanonicalizeOrKeep(found_root), CanonicalizeOrKeep(workspace_root));
}

TEST(WorkspaceAssetPathsTest, ReturnsEmptyWorkspaceRootWhenRequiredMarkersAreIncomplete) {
    ScopedTempWorkspace workspace("incomplete_markers");
    std::error_code ec;
    std::filesystem::remove(workspace.root() / "AGENTS.md", ec);
    const auto nested_cwd = workspace.CreateDirectory("apps/runtime-service");

    ScopedCurrentPath scoped_cwd(nested_cwd);
    const auto found_root = FindWorkspaceRoot();

    EXPECT_TRUE(found_root.empty());
}

TEST(WorkspaceAssetPathsTest, ResolvesRelativeConfigFromNonRootWorkingDirectory) {
    const auto workspace_root = LocateWorkspaceRootFromSourcePath();
    ASSERT_FALSE(workspace_root.empty());
    const auto cli_root = workspace_root / "apps" / "planner-cli";
    ASSERT_TRUE(std::filesystem::exists(cli_root));

    ScopedCurrentPath scoped_cwd(cli_root);
    const auto resolved = ResolveConfigFilePath(
        Siligen::Infrastructure::Configuration::kCanonicalMachineConfigRelativePath);
    const auto expected =
        CanonicalizeOrKeep(workspace_root / Siligen::Infrastructure::Configuration::kCanonicalMachineConfigRelativePath);

    EXPECT_TRUE(std::filesystem::path(resolved).is_absolute());
    EXPECT_EQ(CanonicalizeOrKeep(std::filesystem::path(resolved)), expected);
}

TEST(WorkspaceAssetPathsTest, ResolvesCurrentDirectoryRelativeCanonicalPath) {
    ScopedTempWorkspace workspace("cwd_relative_canonical");
    const auto canonical_path = workspace.WriteFile(
        Siligen::Infrastructure::Configuration::kCanonicalMachineConfigRelativePath,
        "[Machine]\nmode=Mock\n");
    const auto nested_cwd = workspace.CreateDirectory("apps/planner-cli");

    ScopedCurrentPath scoped_cwd(nested_cwd);
    const auto resolution = ResolveConfigFilePathWithBridge("..\\..\\config\\machine\\machine_config.ini");

    EXPECT_FALSE(resolution.fail_fast);
    EXPECT_EQ(resolution.mode, ConfigPathBridgeMode::Canonical);
    EXPECT_EQ(resolution.detail, "cwd-relative-canonical");
    EXPECT_EQ(
        CanonicalizeOrKeep(std::filesystem::path(resolution.resolved_path)),
        CanonicalizeOrKeep(canonical_path));
}

TEST(WorkspaceAssetPathsTest, FailsFastWhenWorkspaceRootCannotBeDetermined) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto temp_root =
        std::filesystem::temp_directory_path() / ("siligen_workspace_missing_root_" + std::to_string(stamp));
    std::error_code ec;
    std::filesystem::create_directories(temp_root / "apps" / "planner-cli", ec);
    ASSERT_FALSE(ec);

    {
        ScopedCurrentPath scoped_cwd(temp_root / "apps" / "planner-cli");
        const auto resolution = ResolveConfigFilePathWithBridge(
            Siligen::Infrastructure::Configuration::kCanonicalMachineConfigRelativePath);

        EXPECT_TRUE(resolution.fail_fast);
        EXPECT_EQ(resolution.mode, ConfigPathBridgeMode::Unresolved);
        EXPECT_EQ(resolution.detail, "workspace-root-not-found");
    }

    std::filesystem::remove_all(temp_root, ec);
}
