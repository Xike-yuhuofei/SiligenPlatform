#include <filesystem>
#include <iomanip>
#include <initializer_list>
#include <iostream>
#include <string>
#include <system_error>

#include "sim/io/result_exporter.h"
#include "sim/io/trajectory_loader.h"
#include "sim/simulation_engine.h"

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

}  // namespace

int main(int argc, char** argv) {
    const std::string input_path =
        (argc > 1)
            ? argv[1]
            : resolveWorkspaceAssetPath({
                  "examples/simulation/sample_trajectory.json",
                  "simulation-engine/examples/sample_trajectory.json",
              });
    const std::string output_path =
        (argc > 2)
            ? argv[2]
            : resolveWorkspaceAssetPath({
                  "examples/replay-data/sample_result.json",
                  "simulation-engine/examples/sample_result.json",
              });

    try {
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(output_path).parent_path(), ec);
        const sim::SimulationContext context = sim::TrajectoryLoader::loadFromJsonFile(input_path);

        sim::SimulationEngine engine;
        const auto result = engine.run(context);
        sim::ResultExporter::writeJsonFile(result, output_path, true);

        std::size_t valve_on_count = 0;
        for (const auto& event : result.valve_events) {
            if (event.state) {
                ++valve_on_count;
            }
        }

        std::cout << std::fixed << std::setprecision(1);
        std::cout << "Cycle Time: " << result.total_time << " s\n";
        std::cout << "Valve ON events: " << valve_on_count << '\n';
        std::cout << "Result JSON: " << output_path << '\n';
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }

    return 0;
}
