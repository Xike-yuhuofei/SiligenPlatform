#pragma once

#include <algorithm>
#include <array>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <system_error>
#include <vector>

namespace Siligen::Infrastructure::Configuration {

inline constexpr char kCanonicalMachineConfigRelativePath[] = "config/machine/machine_config.ini";
inline constexpr char kCanonicalUploadDirectoryRelativePath[] = "uploads/dxf/";

enum class ConfigPathBridgeMode {
    Canonical,
    Absolute,
    Unresolved,
    Error,
};

struct ConfigPathResolution {
    std::string requested_path;
    std::string resolved_path;
    std::string detail;
    ConfigPathBridgeMode mode = ConfigPathBridgeMode::Unresolved;
    bool fail_fast = false;
};

inline std::string CanonicalMachineConfigRelativePath() {
    return kCanonicalMachineConfigRelativePath;
}

inline std::string CanonicalUploadDirectoryRelativePath() {
    return kCanonicalUploadDirectoryRelativePath;
}

inline std::string NormalizeRelativePath(std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    return value;
}

inline const char* ConfigPathBridgeModeName(const ConfigPathBridgeMode mode) noexcept {
    switch (mode) {
        case ConfigPathBridgeMode::Canonical:
            return "canonical";
        case ConfigPathBridgeMode::Absolute:
            return "absolute";
        case ConfigPathBridgeMode::Unresolved:
            return "unresolved";
        case ConfigPathBridgeMode::Error:
            return "error";
        default:
            return "unknown";
    }
}

inline std::filesystem::path CanonicalizeOrKeep(const std::filesystem::path& path) noexcept {
    std::error_code ec;
    auto canonical_path = std::filesystem::weakly_canonical(path, ec);
    if (ec) {
        return path;
    }
    return canonical_path;
}

inline bool IsWorkspaceRootCandidate(const std::filesystem::path& candidate) noexcept {
    std::error_code ec;
    static constexpr std::array<const char*, 3> kRequiredMarkers = {
        "WORKSPACE.md",
        "AGENTS.md",
        "cmake/workspace-layout.env",
    };

    for (const auto* marker : kRequiredMarkers) {
        if (!std::filesystem::exists(candidate / marker, ec) || ec) {
            return false;
        }
        ec.clear();
    }

    return true;
}

inline std::filesystem::path FindWorkspaceRoot() noexcept {
    std::error_code ec;
    std::filesystem::path current = std::filesystem::current_path(ec);
    if (ec) {
        return {};
    }

    std::filesystem::path candidate = CanonicalizeOrKeep(current);
    while (!candidate.empty()) {
        if (IsWorkspaceRootCandidate(candidate)) {
            return candidate;
        }

        auto parent = candidate.parent_path();
        if (parent == candidate) {
            break;
        }
        candidate = parent;
    }

    return {};
}

inline ConfigPathResolution ResolveConfigFilePathWithBridge(const std::string& config_path) noexcept {
    ConfigPathResolution resolution;
    resolution.requested_path = config_path;

    const std::string request_path =
        config_path.empty() ? std::string(kCanonicalMachineConfigRelativePath) : config_path;
    std::filesystem::path request(request_path);
    const auto workspace_root = FindWorkspaceRoot();

    std::filesystem::path canonical_config;
    if (!workspace_root.empty()) {
        canonical_config = CanonicalizeOrKeep(workspace_root / kCanonicalMachineConfigRelativePath);
    }

    if (request.is_absolute()) {
        resolution.resolved_path = CanonicalizeOrKeep(request).string();
        resolution.mode = ConfigPathBridgeMode::Absolute;
        resolution.detail = "absolute-path";
        return resolution;
    }

    if (workspace_root.empty()) {
        resolution.resolved_path = request_path;
        resolution.mode = ConfigPathBridgeMode::Unresolved;
        resolution.detail = "workspace-root-not-found";
        resolution.fail_fast = true;
        return resolution;
    }

    std::error_code cwd_ec;
    const auto current_path = std::filesystem::current_path(cwd_ec);
    if (!cwd_ec) {
        std::error_code exists_ec;
        auto current_relative_candidate = CanonicalizeOrKeep(current_path / request);
        if (std::filesystem::exists(current_relative_candidate, exists_ec) && !exists_ec) {
            if (current_relative_candidate == canonical_config) {
                resolution.resolved_path = canonical_config.string();
                resolution.mode = ConfigPathBridgeMode::Canonical;
                resolution.detail = "cwd-relative-canonical";
                return resolution;
            }
            resolution.resolved_path = current_relative_candidate.string();
            resolution.mode = ConfigPathBridgeMode::Absolute;
            resolution.detail = "cwd-relative-existing-file";
            return resolution;
        }
    }

    const auto workspace_relative_candidate = CanonicalizeOrKeep(workspace_root / request);
    std::error_code exists_ec;
    if (std::filesystem::exists(workspace_relative_candidate, exists_ec) && !exists_ec) {
        if (workspace_relative_candidate == canonical_config) {
            resolution.resolved_path = canonical_config.string();
            resolution.mode = ConfigPathBridgeMode::Canonical;
            resolution.detail = "canonical";
            return resolution;
        }

        resolution.resolved_path = workspace_relative_candidate.string();
        resolution.mode = ConfigPathBridgeMode::Absolute;
        resolution.detail = "workspace-relative-existing-file";
        return resolution;
    }

    resolution.resolved_path = request_path;
    resolution.mode = ConfigPathBridgeMode::Unresolved;
    resolution.detail = "config-file-not-found-under-workspace-root";
    resolution.fail_fast = true;
    return resolution;
}

inline std::string ResolveWorkspaceRelativePath(
    const std::string& canonical_relative_path,
    std::initializer_list<std::string> fallback_relative_paths = {}) noexcept {
    const auto workspace_root = FindWorkspaceRoot();
    if (workspace_root.empty()) {
        return canonical_relative_path;
    }

    std::vector<std::filesystem::path> candidates;
    candidates.emplace_back(workspace_root / canonical_relative_path);
    for (const auto& fallback_relative_path : fallback_relative_paths) {
        candidates.emplace_back(workspace_root / fallback_relative_path);
    }

    std::error_code ec;
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate, ec) && !ec) {
            return CanonicalizeOrKeep(candidate).string();
        }
        ec.clear();
    }

    return (workspace_root / canonical_relative_path).string();
}

inline std::string ResolveConfigFilePath(const std::string& config_path) noexcept {
    return ResolveConfigFilePathWithBridge(config_path).resolved_path;
}

inline std::string ResolveUploadDirectory(
    std::initializer_list<std::string> fallback_relative_paths = {}) noexcept {
    return ResolveWorkspaceRelativePath(kCanonicalUploadDirectoryRelativePath, fallback_relative_paths);
}

}  // namespace Siligen::Infrastructure::Configuration
