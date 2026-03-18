#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "sim/scheme_c/recorder.h"
#include "sim/scheme_c/runtime_bridge.h"
#include "sim/scheme_c/tick_scheduler.h"
#include "sim/scheme_c/virtual_axis_group.h"
#include "sim/scheme_c/virtual_clock.h"
#include "sim/scheme_c/virtual_io.h"

namespace sim::scheme_c {

struct SimulationSessionConfig {
    std::vector<std::string> axis_names{"X", "Y"};
    std::vector<std::string> io_channels{};
    RuntimeBridgeBindings bridge_bindings{};
    Duration tick{std::chrono::milliseconds(1)};
    std::optional<Duration> timeout{};
    TickIndex max_ticks{60000};
};

struct SimulationSessionResult {
    SessionStatus status{SessionStatus::Idle};
    RuntimeBridgeMetadata runtime_bridge{};
    RuntimeBridgeBindings bridge_bindings{};
    RecordingResult recording{};
};

class SimulationSession {
public:
    SimulationSession(SimulationSessionConfig config,
                      std::unique_ptr<RuntimeBridge> runtime_bridge,
                      std::unique_ptr<VirtualAxisGroup> axis_group,
                      std::unique_ptr<VirtualIo> io,
                      std::unique_ptr<Recorder> recorder);

    SimulationSession(SimulationSession&&) noexcept = default;
    SimulationSession& operator=(SimulationSession&&) noexcept = default;

    void start();
    bool advanceOneTick();
    void pause();
    void resume();
    void stop(const std::string& reason = "Stopped by application request.");
    SessionStatus status() const noexcept;
    TickScheduler& scheduler();
    SimulationSessionResult run();
    SimulationSessionResult finish();

    VirtualClock& clock();
    RuntimeBridge& runtimeBridge();
    VirtualAxisGroup& axisGroup();
    VirtualIo& io();
    Recorder& recorder();

private:
    SimulationSessionConfig config_{};
    VirtualClock clock_;
    std::unique_ptr<RuntimeBridge> runtime_bridge_;
    std::unique_ptr<VirtualAxisGroup> axis_group_;
    std::unique_ptr<VirtualIo> io_;
    std::unique_ptr<Recorder> recorder_;
    TickScheduler scheduler_{};
    SessionStatus status_{SessionStatus::Idle};
    bool bridge_completed_{false};
    bool finalized_{false};
    std::size_t internal_task_count_{0};
    std::string termination_reason_{"not-run"};
    SimulationSessionResult final_result_{};

    void configureScheduler();
    void finalizeIfNeeded();
    void requestStopInternal(SessionStatus terminal_status,
                             const std::string& termination_reason,
                             const std::string& source,
                             const std::string& message);
};

SimulationSession createBaselineSession(const SimulationSessionConfig& config = {});

}  // namespace sim::scheme_c
