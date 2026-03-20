#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <string>
#include <system_error>

#include "sim/io/result_exporter.h"
#include "sim/scheme_c/application_runner.h"

namespace {

std::filesystem::path findWorkspaceRoot() noexcept {
    std::error_code ec;
    auto current = std::filesystem::current_path(ec);
    if (ec) {
        return std::filesystem::path(".");
    }

    std::filesystem::path candidate = current;
    std::filesystem::path fallback = current;
    bool fallback_found = false;

    for (int level = 0; level < 10; ++level) {
        if (std::filesystem::exists(candidate / "WORKSPACE.md", ec) && !ec) {
            return candidate;
        }
        ec.clear();

        if (!fallback_found &&
            ((std::filesystem::exists(candidate / "CMakeLists.txt", ec) && !ec) ||
             (std::filesystem::exists(candidate / ".git", ec) && !ec))) {
            fallback = candidate;
            fallback_found = true;
        }
        ec.clear();

        auto parent = candidate.parent_path();
        if (parent == candidate) {
            break;
        }
        candidate = parent;
    }

    return fallback_found ? fallback : current;
}

std::string resolveWorkspaceAssetPath(std::initializer_list<std::string> relative_candidates) {
    const auto workspace_root = findWorkspaceRoot();
    std::error_code ec;

    for (const auto& relative_candidate : relative_candidates) {
        const auto candidate = workspace_root / relative_candidate;
        if (std::filesystem::exists(candidate, ec) && !ec) {
            return candidate.string();
        }
        ec.clear();
    }

    return (workspace_root / *relative_candidates.begin()).string();
}

std::string sessionStatusToString(sim::scheme_c::SessionStatus status) {
    switch (status) {
    case sim::scheme_c::SessionStatus::Idle:
        return "idle";
    case sim::scheme_c::SessionStatus::Running:
        return "running";
    case sim::scheme_c::SessionStatus::Paused:
        return "paused";
    case sim::scheme_c::SessionStatus::Completed:
        return "completed";
    case sim::scheme_c::SessionStatus::Stopped:
        return "stopped";
    case sim::scheme_c::SessionStatus::TimedOut:
        return "timed_out";
    case sim::scheme_c::SessionStatus::Failed:
        return "failed";
    }

    return "unknown";
}

int exitCodeForStatus(sim::scheme_c::SessionStatus status) {
    switch (status) {
    case sim::scheme_c::SessionStatus::Completed:
    case sim::scheme_c::SessionStatus::Stopped:
        return 0;
    case sim::scheme_c::SessionStatus::TimedOut:
        return 3;
    case sim::scheme_c::SessionStatus::Failed:
        return 2;
    default:
        return 1;
    }
}

}  // namespace

int main(int argc, char** argv) {
    const std::string input_path =
        (argc > 1)
            ? argv[1]
            : resolveWorkspaceAssetPath({
                  "examples/simulation/rect_diag.simulation-input.json",
                  "examples/simulation/sample_trajectory.json",
              });
    const std::string output_path =
        (argc > 2)
            ? argv[2]
            : resolveWorkspaceAssetPath({
                  "examples/replay-data/rect_diag.scheme_c.recording.json",
              });

    try {
        const auto result = sim::scheme_c::runMinimalClosedLoopFile(input_path);

        std::error_code ec;
        const auto output_parent = std::filesystem::path(output_path).parent_path();
        if (!output_parent.empty()) {
            std::filesystem::create_directories(output_parent, ec);
        }

        sim::ResultExporter::writeJsonFile(result, output_path, true);

        std::cout
            << "Status: " << sessionStatusToString(result.status) << '\n'
            << "Termination: " << result.recording.summary.termination_reason << '\n'
            << "Ticks: " << result.recording.ticks_executed << '\n'
            << "Output JSON: " << output_path << '\n';

        return exitCodeForStatus(result.status);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
