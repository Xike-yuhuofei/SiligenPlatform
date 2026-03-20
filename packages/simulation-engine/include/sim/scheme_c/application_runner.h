#pragma once

#include <optional>
#include <string>
#include <vector>

#include "sim/scheme_c/runtime_bridge.h"
#include "sim/scheme_c/simulation_session.h"

namespace sim::scheme_c {

struct StopDirective {
    std::optional<TickIndex> after_tick{};
    std::optional<Duration> after_time{};
    bool immediate{true};
    bool clear_pending_commands{true};
};

struct CanonicalSimulationInput {
    std::vector<std::string> axis_names{"X", "Y"};
    std::vector<std::string> io_channels{};
    RuntimeBridgeBindings bridge_bindings{};
    MotionRealismConfig motion_realism{};
    Duration tick{std::chrono::milliseconds(1)};
    std::optional<Duration> timeout{};
    TickIndex max_ticks{600001};
    bool home_before_run{true};
    std::optional<RuntimeBridgeMoveCommand> initial_move{};
    RuntimeBridgePathCommand path{};
    DeterministicReplayPlan replay_plan{};
    std::optional<StopDirective> stop{};
};

CanonicalSimulationInput loadCanonicalSimulationInputJson(
    const std::string& json_text,
    const std::string& source_name = {});
CanonicalSimulationInput loadCanonicalSimulationInputFile(const std::string& path);

SimulationSessionResult runMinimalClosedLoop(const CanonicalSimulationInput& input);
SimulationSessionResult runMinimalClosedLoopFile(const std::string& input_path);

}  // namespace sim::scheme_c
