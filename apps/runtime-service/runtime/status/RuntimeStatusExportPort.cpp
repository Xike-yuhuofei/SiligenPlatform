#include "runtime/status/RuntimeStatusExportPort.h"

#include "shared/types/Error.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef GetMessage
#undef GetMessage
#endif
#endif

namespace Siligen::Runtime::Service::Status {
namespace {

using IRuntimeSupervisionPort = Siligen::RuntimeExecution::Contracts::System::IRuntimeSupervisionPort;
using RuntimeAxisStatusExportSnapshot =
    Siligen::RuntimeExecution::Contracts::System::RuntimeAxisStatusExportSnapshot;
using RuntimeIdentityExportSnapshot =
    Siligen::RuntimeExecution::Contracts::System::RuntimeIdentityExportSnapshot;
using RuntimeJobExecutionExportSnapshot =
    Siligen::RuntimeExecution::Contracts::System::RuntimeJobExecutionExportSnapshot;
using RuntimeActionCapabilitiesExportSnapshot =
    Siligen::RuntimeExecution::Contracts::System::RuntimeActionCapabilitiesExportSnapshot;
using RuntimeSafetyBoundaryExportSnapshot =
    Siligen::RuntimeExecution::Contracts::System::RuntimeSafetyBoundaryExportSnapshot;
using RuntimeStatusExportSnapshot = Siligen::RuntimeExecution::Contracts::System::RuntimeStatusExportSnapshot;
using DispenserValveStatus = Siligen::Domain::Dispensing::Ports::DispenserValveStatus;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

constexpr const char* kErrorSource = "RuntimeStatusExportPort";
constexpr const char* kRuntimeProtocolVersion = "siligen.application/1.0";
constexpr const char* kPreviewSnapshotContract =
    "planned_glue_snapshot.glue_points+execution_trajectory_snapshot.polyline";

Result<std::shared_ptr<IRuntimeSupervisionPort>> EnsureSupervisionPort(
    const std::shared_ptr<IRuntimeSupervisionPort>& runtime_supervision_port) {
    if (!runtime_supervision_port) {
        return Result<std::shared_ptr<IRuntimeSupervisionPort>>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "runtime supervision port not initialized",
            kErrorSource));
    }
    return Result<std::shared_ptr<IRuntimeSupervisionPort>>::Success(runtime_supervision_port);
}

std::string NormalizePathForExport(const std::filesystem::path& path) {
    if (path.empty()) {
        return {};
    }

    std::error_code ec;
    auto canonical_path = std::filesystem::weakly_canonical(path, ec);
    if (!ec) {
        return canonical_path.string();
    }

    canonical_path = std::filesystem::absolute(path, ec);
    if (!ec) {
        return canonical_path.string();
    }

    return path.lexically_normal().string();
}

std::filesystem::path ResolveRuntimeExecutablePath() {
#if defined(_WIN32)
    std::wstring buffer(MAX_PATH, L'\0');
    while (true) {
        const auto size = static_cast<DWORD>(buffer.size());
        const DWORD length = ::GetModuleFileNameW(nullptr, buffer.data(), size);
        if (length == 0) {
            return {};
        }
        if (length < size - 1) {
            buffer.resize(length);
            return std::filesystem::path(buffer);
        }
        buffer.resize(buffer.size() * 2);
    }
#else
    std::error_code ec;
    const auto proc_self_exe = std::filesystem::path("/proc/self/exe");
    if (std::filesystem::exists(proc_self_exe, ec)) {
        const auto executable_path = std::filesystem::read_symlink(proc_self_exe, ec);
        if (!ec) {
            return executable_path;
        }
    }
    return {};
#endif
}

std::filesystem::path ResolveRuntimeWorkingDirectoryPath() {
    std::error_code ec;
    const auto working_directory = std::filesystem::current_path(ec);
    if (ec) {
        return {};
    }
    return working_directory;
}

RuntimeIdentityExportSnapshot BuildRuntimeIdentitySnapshot() {
    RuntimeIdentityExportSnapshot snapshot;
    snapshot.executable_path = NormalizePathForExport(ResolveRuntimeExecutablePath());
    snapshot.working_directory = NormalizePathForExport(ResolveRuntimeWorkingDirectoryPath());
    snapshot.protocol_version = kRuntimeProtocolVersion;
    snapshot.preview_snapshot_contract = kPreviewSnapshotContract;
    return snapshot;
}

RuntimeAxisStatusExportSnapshot BuildAxisStatusSnapshot(const Siligen::Domain::Motion::Ports::MotionStatus& status) {
    RuntimeAxisStatusExportSnapshot snapshot;
    snapshot.position = status.position.x;
    snapshot.velocity = status.velocity;
    snapshot.enabled = status.enabled;
    snapshot.homed = status.homing_state == "homed";
    snapshot.homing_state = status.homing_state;
    return snapshot;
}

bool IsTerminalJobState(const std::string& state) {
    return state == "completed" || state == "failed" || state == "cancelled";
}

bool IsInactiveJobState(const std::string& state) {
    return state == "idle" || IsTerminalJobState(state);
}

RuntimeJobExecutionExportSnapshot BuildIdleJobExecutionSnapshot() {
    RuntimeJobExecutionExportSnapshot snapshot;
    snapshot.state = "idle";
    return snapshot;
}

RuntimeJobExecutionExportSnapshot BuildUnknownJobExecutionSnapshot(
    const std::string& job_id,
    const std::string& error_message) {
    RuntimeJobExecutionExportSnapshot snapshot;
    snapshot.job_id = job_id;
    snapshot.state = "unknown";
    snapshot.error_message = error_message;
    return snapshot;
}

bool ResolveEstopKnown(const RuntimeStatusExportSnapshot& snapshot) {
    return snapshot.effective_interlocks.estop_known || snapshot.io.estop_known;
}

bool ResolveEstopActive(const RuntimeStatusExportSnapshot& snapshot) {
    if (snapshot.effective_interlocks.estop_known) {
        return snapshot.effective_interlocks.estop_active;
    }
    return snapshot.io.estop;
}

bool ResolveDoorKnown(const RuntimeStatusExportSnapshot& snapshot) {
    return snapshot.effective_interlocks.door_open_known || snapshot.io.door_known;
}

bool ResolveDoorOpenActive(const RuntimeStatusExportSnapshot& snapshot) {
    if (snapshot.effective_interlocks.door_open_known) {
        return snapshot.effective_interlocks.door_open_active;
    }
    return snapshot.io.door;
}

bool IsUnknownSafetyBlockingReason(const std::string& reason) {
    return reason == "estop_unknown" || reason == "door_unknown";
}

bool IsBackendOnline(const RuntimeStatusExportSnapshot& snapshot) {
    return snapshot.connected && snapshot.connection_state == "connected";
}

RuntimeSafetyBoundaryExportSnapshot BuildSafetyBoundarySnapshot(const RuntimeStatusExportSnapshot& snapshot) {
    RuntimeSafetyBoundaryExportSnapshot safety_boundary;
    safety_boundary.estop_known = ResolveEstopKnown(snapshot);
    safety_boundary.estop_active = ResolveEstopActive(snapshot);
    safety_boundary.door_open_known = ResolveDoorKnown(snapshot);
    safety_boundary.door_open_active = ResolveDoorOpenActive(snapshot);
    safety_boundary.interlock_latched = snapshot.interlock_latched;

    std::vector<std::string> blocking_reasons;
    if (!safety_boundary.estop_known) {
        blocking_reasons.emplace_back("estop_unknown");
    } else if (safety_boundary.estop_active) {
        blocking_reasons.emplace_back("estop_active");
    }

    if (!safety_boundary.door_open_known) {
        blocking_reasons.emplace_back("door_unknown");
    } else if (safety_boundary.door_open_active) {
        blocking_reasons.emplace_back("door_open_active");
    }

    if (safety_boundary.interlock_latched) {
        blocking_reasons.emplace_back("interlock_latched");
    }

    safety_boundary.motion_permitted = blocking_reasons.empty();
    safety_boundary.process_output_permitted =
        safety_boundary.motion_permitted && snapshot.device_mode == "production" && !snapshot.job_execution.dry_run;
    safety_boundary.blocking_reasons = std::move(blocking_reasons);

    bool has_unknown_reason = false;
    for (const auto& reason : safety_boundary.blocking_reasons) {
        if (IsUnknownSafetyBlockingReason(reason)) {
            has_unknown_reason = true;
            break;
        }
    }

    if (has_unknown_reason) {
        safety_boundary.state = "unknown";
    } else if (!safety_boundary.blocking_reasons.empty()) {
        safety_boundary.state = "blocked";
    } else {
        safety_boundary.state = "safe";
    }

    return safety_boundary;
}

RuntimeActionCapabilitiesExportSnapshot BuildActionCapabilitiesSnapshot(
    const RuntimeStatusExportSnapshot& snapshot,
    const std::optional<DispenserValveStatus>& dispenser_status) {
    RuntimeActionCapabilitiesExportSnapshot action_capabilities;
    const bool backend_online = IsBackendOnline(snapshot);
    const bool active_job_present =
        !snapshot.job_execution.job_id.empty() && !IsInactiveJobState(snapshot.job_execution.state);
    action_capabilities.motion_commands_permitted =
        backend_online && snapshot.safety_boundary.motion_permitted;
    action_capabilities.manual_output_commands_permitted =
        backend_online && snapshot.safety_boundary.process_output_permitted;
    action_capabilities.manual_dispenser_pause_permitted =
        backend_online &&
        snapshot.safety_boundary.process_output_permitted &&
        !active_job_present &&
        dispenser_status.has_value() &&
        *dispenser_status == DispenserValveStatus::Running;
    action_capabilities.manual_dispenser_resume_permitted =
        backend_online &&
        snapshot.safety_boundary.process_output_permitted &&
        !active_job_present &&
        dispenser_status.has_value() &&
        *dispenser_status == DispenserValveStatus::Paused;
    action_capabilities.active_job_present = active_job_present;
    action_capabilities.estop_reset_permitted =
        backend_online && snapshot.supervision.current_state == "Estop";
    return action_capabilities;
}

}  // namespace

RuntimeStatusExportPort::RuntimeStatusExportPort(
    std::shared_ptr<IRuntimeSupervisionPort> runtime_supervision_port,
    RuntimeMotionStatusReader motion_status_reader,
    RuntimeDispenserStatusReader dispenser_status_reader,
    RuntimeJobExecutionStatusReader job_execution_status_reader)
    : runtime_supervision_port_(std::move(runtime_supervision_port)),
      motion_status_reader_(std::move(motion_status_reader)),
      dispenser_status_reader_(std::move(dispenser_status_reader)),
      job_execution_status_reader_(std::move(job_execution_status_reader)) {}

Result<RuntimeStatusExportSnapshot> RuntimeStatusExportPort::ReadSnapshot() const {
    auto supervision_port_result = EnsureSupervisionPort(runtime_supervision_port_);
    if (supervision_port_result.IsError()) {
        return Result<RuntimeStatusExportSnapshot>::Failure(supervision_port_result.GetError());
    }

    auto supervision_result = runtime_supervision_port_->ReadSnapshot();
    if (supervision_result.IsError()) {
        return Result<RuntimeStatusExportSnapshot>::Failure(supervision_result.GetError());
    }

    RuntimeStatusExportSnapshot snapshot;
    const auto& supervision = supervision_result.Value();
    snapshot.connected = supervision.connected;
    snapshot.connection_state = supervision.connection_state;
    snapshot.machine_state = supervision.supervision.current_state;
    snapshot.machine_state_reason = supervision.supervision.state_reason;
    snapshot.runtime_identity = BuildRuntimeIdentitySnapshot();
    snapshot.interlock_latched = supervision.interlock_latched;
    snapshot.io = supervision.io;
    snapshot.effective_interlocks = supervision.effective_interlocks;
    snapshot.supervision = supervision.supervision;
    snapshot.job_execution = BuildIdleJobExecutionSnapshot();

    if (!supervision.active_job_id.empty()) {
        {
            std::lock_guard<std::mutex> lock(job_execution_mutex_);
            last_observed_job_id_ = supervision.active_job_id;
            cached_terminal_job_execution_.reset();
        }

        if (job_execution_status_reader_.read_job_status) {
            auto job_status_result = job_execution_status_reader_.read_job_status(supervision.active_job_id);
            if (job_status_result.IsSuccess()) {
                snapshot.job_execution = job_status_result.Value();
            } else {
                snapshot.job_execution = BuildUnknownJobExecutionSnapshot(
                    supervision.active_job_id,
                    job_status_result.GetError().GetMessage());
            }
        } else {
            snapshot.job_execution = BuildUnknownJobExecutionSnapshot(
                supervision.active_job_id,
                "job execution authority unavailable");
        }
    } else {
        std::optional<RuntimeJobExecutionExportSnapshot> cached_terminal_job_execution;
        std::string last_observed_job_id;
        {
            std::lock_guard<std::mutex> lock(job_execution_mutex_);
            cached_terminal_job_execution = cached_terminal_job_execution_;
            last_observed_job_id = last_observed_job_id_;
        }

        if (cached_terminal_job_execution.has_value()) {
            snapshot.job_execution = cached_terminal_job_execution.value();
        } else if (!last_observed_job_id.empty() && job_execution_status_reader_.read_job_status) {
            auto job_status_result = job_execution_status_reader_.read_job_status(last_observed_job_id);
            if (job_status_result.IsSuccess() && IsTerminalJobState(job_status_result.Value().state)) {
                snapshot.job_execution = job_status_result.Value();
                std::lock_guard<std::mutex> lock(job_execution_mutex_);
                cached_terminal_job_execution_ = snapshot.job_execution;
            } else {
                std::lock_guard<std::mutex> lock(job_execution_mutex_);
                last_observed_job_id_.clear();
            }
        }
    }
    snapshot.device_mode = snapshot.job_execution.dry_run ? "test" : "production";

    std::optional<DispenserValveStatus> dispenser_status_for_action_capabilities;

    if (snapshot.connected && motion_status_reader_.read_all_axes_motion_status) {
        auto all_status_result = motion_status_reader_.read_all_axes_motion_status();
        if (all_status_result.IsSuccess()) {
            const auto& statuses = all_status_result.Value();
            const char* axis_names[] = {"X", "Y", "Z", "U"};
            for (size_t i = 0; i < statuses.size() && i < 4; ++i) {
                snapshot.axes[axis_names[i]] = BuildAxisStatusSnapshot(statuses[i]);
            }
        }
    }

    if (snapshot.connected && motion_status_reader_.read_current_position) {
        auto position_result = motion_status_reader_.read_current_position();
        if (position_result.IsSuccess()) {
            const auto position = position_result.Value();
            snapshot.has_position = true;
            snapshot.position.x = position.x;
            snapshot.position.y = position.y;
        }
    }

    if (dispenser_status_reader_.read_dispenser_status) {
        auto dispenser_result = dispenser_status_reader_.read_dispenser_status();
        if (dispenser_result.IsSuccess()) {
            const auto& dispenser = dispenser_result.Value();
            dispenser_status_for_action_capabilities = dispenser.status;
            snapshot.dispenser.valve_open = dispenser.IsRunning();
            snapshot.dispenser.completedCount = dispenser.completedCount;
            snapshot.dispenser.totalCount = dispenser.totalCount;
            snapshot.dispenser.progress = dispenser.progress;
        }
    }

    if (dispenser_status_reader_.read_supply_status) {
        auto supply_result = dispenser_status_reader_.read_supply_status();
        if (supply_result.IsSuccess()) {
            snapshot.dispenser.supply_open = supply_result.Value().IsOpen();
        }
    }

    snapshot.safety_boundary = BuildSafetyBoundarySnapshot(snapshot);
    snapshot.action_capabilities =
        BuildActionCapabilitiesSnapshot(snapshot, dispenser_status_for_action_capabilities);

    return Result<RuntimeStatusExportSnapshot>::Success(std::move(snapshot));
}

}  // namespace Siligen::Runtime::Service::Status
