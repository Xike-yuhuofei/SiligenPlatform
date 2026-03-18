#include "sim/scheme_c/simulation_session.h"

#include <utility>

namespace sim::scheme_c {

SimulationSession::SimulationSession(SimulationSessionConfig config,
                                     std::unique_ptr<RuntimeBridge> runtime_bridge,
                                     std::unique_ptr<VirtualAxisGroup> axis_group,
                                     std::unique_ptr<VirtualIo> io,
                                     std::unique_ptr<Recorder> recorder)
    : config_(std::move(config)),
      clock_(config_.tick),
      runtime_bridge_(std::move(runtime_bridge)),
      axis_group_(std::move(axis_group)),
      io_(std::move(io)),
      recorder_(std::move(recorder)) {}

void SimulationSession::start() {
    finalized_ = false;
    final_result_ = {};
    termination_reason_ = "not-run";
    bridge_completed_ = false;

    if (!runtime_bridge_ || !axis_group_ || !io_ || !recorder_) {
        status_ = SessionStatus::Failed;
        termination_reason_ = "session_dependencies_missing";
        return;
    }

    clock_.reset();
    clock_.start(config_.timeout);

    axis_group_->setAxes(config_.axis_names);
    io_->setChannels(config_.io_channels);
    scheduler_.clear();

    runtime_bridge_->attach(*axis_group_, *io_, recorder_.get());
    runtime_bridge_->initialize(clock_.current());

    configureScheduler();
    status_ = SessionStatus::Running;
    termination_reason_ = "bridge_running";
    recorder_->recordEvent(clock_.current(), "SimulationSession", "Session started.");
}

bool SimulationSession::advanceOneTick() {
    if (status_ == SessionStatus::Paused) {
        return true;
    }
    if (status_ != SessionStatus::Running) {
        return false;
    }

    const TickInfo tick = clock_.current();
    bridge_completed_ = false;
    scheduler_.dispatch(tick);

    const auto* control = runtimeBridgeControl(*runtime_bridge_);
    const bool bridge_has_work =
        !bridge_completed_ || (control != nullptr && control->hasPendingCommands());
    const bool has_user_tasks = scheduler_.pendingTaskCount() > internal_task_count_;
    const bool has_command_failure =
        control != nullptr && control->hasCommandFailure() && !control->hasPendingCommands();

    try {
        clock_.advance();
    } catch (const VirtualTimeError& error) {
        if (error.code() == VirtualTimeErrorCode::TimeoutReached) {
            requestStopInternal(
                SessionStatus::TimedOut,
                "logical_timeout",
                "SimulationSession",
                "Stopped after reaching logical timeout.");
            return false;
        }

        requestStopInternal(
            SessionStatus::Failed,
            "virtual_clock_error",
            "SimulationSession",
            error.what());
        return false;
    }

    if (has_command_failure) {
        requestStopInternal(
            SessionStatus::Failed,
            "bridge_command_failed",
            "RuntimeBridge",
            "Runtime bridge command failed.");
        return false;
    }

    if (!bridge_has_work && !has_user_tasks) {
        status_ = SessionStatus::Completed;
        termination_reason_ = "bridge_completed";
        return false;
    }

    if (clock_.current().tick_index >= config_.max_ticks) {
        requestStopInternal(
            SessionStatus::Stopped,
            "max_ticks_reached",
            "SimulationSession",
            "Stopped after reaching max_ticks before bridge completion.");
        return false;
    }

    return true;
}

void SimulationSession::pause() {
    if (status_ != SessionStatus::Running) {
        return;
    }

    clock_.pause();
    status_ = SessionStatus::Paused;
    recorder_->recordEvent(clock_.current(), "SimulationSession", "Session paused.");
}

void SimulationSession::resume() {
    if (status_ != SessionStatus::Paused) {
        return;
    }

    clock_.resume();
    status_ = SessionStatus::Running;
    recorder_->recordEvent(clock_.current(), "SimulationSession", "Session resumed.");
}

void SimulationSession::stop(const std::string& reason) {
    if (status_ == SessionStatus::Completed ||
        status_ == SessionStatus::Stopped ||
        status_ == SessionStatus::TimedOut ||
        status_ == SessionStatus::Failed) {
        return;
    }

    requestStopInternal(SessionStatus::Stopped, "application_requested_stop", "SimulationSession", reason);
}

SessionStatus SimulationSession::status() const noexcept {
    return status_;
}

TickScheduler& SimulationSession::scheduler() {
    return scheduler_;
}

SimulationSessionResult SimulationSession::run() {
    if (status_ == SessionStatus::Idle) {
        start();
    }

    while (status_ == SessionStatus::Running) {
        if (!advanceOneTick()) {
            break;
        }
    }

    return finish();
}

SimulationSessionResult SimulationSession::finish() {
    finalizeIfNeeded();
    return final_result_;
}

VirtualClock& SimulationSession::clock() {
    return clock_;
}

RuntimeBridge& SimulationSession::runtimeBridge() {
    return *runtime_bridge_;
}

VirtualAxisGroup& SimulationSession::axisGroup() {
    return *axis_group_;
}

VirtualIo& SimulationSession::io() {
    return *io_;
}

Recorder& SimulationSession::recorder() {
    return *recorder_;
}

void SimulationSession::configureScheduler() {
    scheduler_.scheduleRecurring(
        TickTaskPhase::RuntimeBridge,
        [this](const TickInfo& tick) {
            bridge_completed_ = !runtime_bridge_->advance(tick);
        },
        "runtime_bridge");

    scheduler_.scheduleRecurring(
        TickTaskPhase::VirtualAxisGroup,
        [this](const TickInfo& tick) {
            axis_group_->advance(tick);
        },
        "virtual_axis_group");

    scheduler_.scheduleRecurring(
        TickTaskPhase::VirtualIo,
        [this](const TickInfo& tick) {
            io_->advance(tick);
        },
        "virtual_io");

    scheduler_.scheduleRecurring(
        TickTaskPhase::Recording,
        [this](const TickInfo& tick) {
            recorder_->recordSnapshot(tick, axis_group_->snapshot(), io_->snapshot());
        },
        "recording");

    internal_task_count_ = scheduler_.pendingTaskCount();
}

void SimulationSession::finalizeIfNeeded() {
    if (finalized_) {
        return;
    }

    final_result_.status = status_;
    if (runtime_bridge_) {
        final_result_.runtime_bridge = runtime_bridge_->metadata();
        final_result_.bridge_bindings = runtime_bridge_->bindings();
    }

    if (recorder_) {
        final_result_.recording = recorder_->finish(
            clock_.current(),
            axis_group_ ? axis_group_->snapshot() : std::vector<AxisState>{},
            io_ ? io_->snapshot() : std::vector<IoState>{});
        finalizeRecordingResult(final_result_.recording, status_, termination_reason_);
    } else {
        final_result_.recording = makeEmptyRecordingResult(status_, termination_reason_);
    }

    finalized_ = true;
}

void SimulationSession::requestStopInternal(SessionStatus terminal_status,
                                            const std::string& termination_reason,
                                            const std::string& source,
                                            const std::string& message) {
    termination_reason_ = termination_reason;
    if (runtime_bridge_) {
        runtime_bridge_->requestStop();
    }
    if (recorder_) {
        recorder_->recordEvent(clock_.current(), source, message);
    }
    status_ = terminal_status;
}

SimulationSession createBaselineSession(const SimulationSessionConfig& config) {
    RuntimeBridgeBindings bindings = config.bridge_bindings;
    if (bindings.axis_bindings.empty() && bindings.io_bindings.empty()) {
        bindings = makeDefaultRuntimeBridgeBindings(config.axis_names, config.io_channels);
    }

    return SimulationSession(
        config,
        createRuntimeBridge(bindings),
        createInMemoryVirtualAxisGroup(config.axis_names),
        createInMemoryVirtualIo(config.io_channels),
        createTimelineRecorder());
}

}  // namespace sim::scheme_c
