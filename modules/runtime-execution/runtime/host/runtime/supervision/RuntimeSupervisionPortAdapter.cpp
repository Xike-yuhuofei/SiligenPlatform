#include "runtime/supervision/RuntimeSupervisionPortAdapter.h"

#include "shared/types/Error.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <utility>

namespace Siligen::Runtime::Host::Supervision {
namespace {

using Siligen::RuntimeExecution::Contracts::System::EffectiveInterlocksStatus;
using Siligen::RuntimeExecution::Contracts::System::RuntimeSupervisionSnapshot;
using Siligen::RuntimeExecution::Contracts::System::SupervisionStatusSnapshot;
using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

constexpr const char* kErrorSource = "RuntimeSupervisionPortAdapter";

Result<std::shared_ptr<IRuntimeSupervisionBackend>> EnsureBackend(
    const std::shared_ptr<IRuntimeSupervisionBackend>& backend) {
    if (!backend) {
        return Result<std::shared_ptr<IRuntimeSupervisionBackend>>::Failure(Error(
            ErrorCode::PORT_NOT_INITIALIZED,
            "runtime supervision backend not initialized",
            kErrorSource));
    }
    return Result<std::shared_ptr<IRuntimeSupervisionBackend>>::Success(backend);
}

std::string ToConnectionStateLabel(Siligen::Device::Contracts::State::DeviceConnectionState state) {
    using State = Siligen::Device::Contracts::State::DeviceConnectionState;
    switch (state) {
        case State::Connected:
            return "connected";
        case State::Connecting:
            return "connecting";
        case State::Error:
            return "error";
        case State::Disconnected:
        default:
            return "disconnected";
    }
}

std::string ToIso8601UtcNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm utc_tm{};
#ifdef _WIN32
    gmtime_s(&utc_tm, &t);
#else
    gmtime_r(&t, &utc_tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

EffectiveInterlocksStatus BuildEffectiveInterlocksSnapshot(const RuntimeSupervisionInputs& inputs) {
    EffectiveInterlocksStatus snapshot;
    snapshot.estop_active = inputs.io.estop;
    snapshot.estop_known = inputs.io.estop_known;
    snapshot.door_open_active = inputs.io.door_known && inputs.io.door;
    snapshot.door_open_known = inputs.io.door_known;
    snapshot.home_boundary_x_active = inputs.home_boundary_x_active;
    snapshot.home_boundary_y_active = inputs.home_boundary_y_active;
    if (inputs.home_boundary_x_active) {
        snapshot.positive_escape_only_axes.push_back("X");
    }
    if (inputs.home_boundary_y_active) {
        snapshot.positive_escape_only_axes.push_back("Y");
    }
    snapshot.sources["estop"] =
        inputs.estop_state_known ? "system_interlock" : (inputs.connected ? "motion_status" : "unknown");
    snapshot.sources["door_open"] = inputs.io.door_known ? "dispensing_interlock" : "unknown";
    snapshot.sources["home_boundary_x"] = inputs.home_boundary_x_active ? "motion_home_signal" : "none";
    snapshot.sources["home_boundary_y"] = inputs.home_boundary_y_active ? "motion_home_signal" : "none";
    return snapshot;
}

SupervisionStatusSnapshot BuildSupervisionSnapshot(const RuntimeSupervisionInputs& inputs) {
    SupervisionStatusSnapshot snapshot;

    std::string current_state = "Disconnected";
    std::string state_reason = inputs.connected ? "idle" : "motion_status_unavailable";
    std::string connection_state = inputs.connected ? "connected" : "disconnected";

    if (inputs.has_hardware_connection_port) {
        if (inputs.heartbeat_status.is_degraded) {
            connection_state = "degraded";
            current_state = "Degraded";
            state_reason = "heartbeat_degraded";
        } else {
            connection_state = ToConnectionStateLabel(inputs.connection_info.state);
            switch (inputs.connection_info.state) {
                case Siligen::Device::Contracts::State::DeviceConnectionState::Connected:
                    current_state = "Idle";
                    state_reason = "idle";
                    break;
                case Siligen::Device::Contracts::State::DeviceConnectionState::Connecting:
                    current_state = "Unknown";
                    state_reason = "hardware_connecting";
                    break;
                case Siligen::Device::Contracts::State::DeviceConnectionState::Error:
                    current_state = "Fault";
                    state_reason = "hardware_connection_error";
                    break;
                case Siligen::Device::Contracts::State::DeviceConnectionState::Disconnected:
                default:
                    current_state = "Disconnected";
                    state_reason = "hardware_disconnected";
                    break;
            }
        }
    }

    if (inputs.connected) {
        current_state = "Idle";
        if (!inputs.active_job_id.empty()) {
            if (inputs.active_job_status_available) {
                if (inputs.active_job_state == "paused") {
                    current_state = "Paused";
                    state_reason = "job_paused";
                } else if (inputs.active_job_state == "pending" ||
                           inputs.active_job_state == "running" ||
                           inputs.active_job_state == "stopping") {
                    current_state = "Running";
                    state_reason = std::string("job_") + inputs.active_job_state;
                } else if (inputs.active_job_state == "failed") {
                    current_state = "Fault";
                    state_reason = "job_failed";
                }
            } else {
                state_reason = "job_status_unavailable";
            }
        }

        if (current_state == "Idle" && inputs.any_axis_moving) {
            current_state = "Running";
            state_reason = "axis_motion";
        }
        if (inputs.any_axis_fault) {
            current_state = "Fault";
            state_reason = "axis_fault";
        }
        if (inputs.io.estop) {
            current_state = "Estop";
            state_reason = "interlock_estop";
        } else if (inputs.io.door_known && inputs.io.door && current_state != "Fault") {
            current_state = "Fault";
            state_reason = "interlock_door_open";
        } else if (!inputs.io.door_known) {
            current_state = "Unknown";
            state_reason = "door_signal_unknown";
        }
    }

    snapshot.current_state = current_state;
    snapshot.requested_state = current_state;
    snapshot.state_reason = state_reason;
    snapshot.updated_at = ToIso8601UtcNow();

    if (connection_state == "connecting") {
        snapshot.requested_state = "Idle";
        snapshot.state_change_in_process = true;
    }
    if (inputs.connected && inputs.io.estop && state_reason != "interlock_estop") {
        snapshot.requested_state = "Estop";
        snapshot.state_change_in_process = true;
    } else if (inputs.connected && inputs.io.door_known && inputs.io.door && state_reason != "interlock_door_open") {
        snapshot.requested_state = "Fault";
        snapshot.state_change_in_process = true;
    } else if (inputs.connected && inputs.io.door_known && !inputs.io.door && state_reason == "interlock_door_open") {
        snapshot.requested_state = "Idle";
        snapshot.state_change_in_process = true;
    }

    if (connection_state == "degraded") {
        snapshot.failure_code = "heartbeat_degraded";
    } else if (current_state == "Fault" || current_state == "Estop" || current_state == "Degraded") {
        snapshot.failure_code = state_reason;
    }
    snapshot.failure_stage = snapshot.failure_code.empty() ? "" : "runtime_status";

    return snapshot;
}

}  // namespace

RuntimeSupervisionPortAdapter::RuntimeSupervisionPortAdapter(std::shared_ptr<IRuntimeSupervisionBackend> backend)
    : backend_(std::move(backend)) {}

Result<RuntimeSupervisionSnapshot> RuntimeSupervisionPortAdapter::ReadSnapshot() const {
    auto backend_result = EnsureBackend(backend_);
    if (backend_result.IsError()) {
        return Result<RuntimeSupervisionSnapshot>::Failure(backend_result.GetError());
    }

    auto inputs_result = backend_->ReadInputs();
    if (inputs_result.IsError()) {
        return Result<RuntimeSupervisionSnapshot>::Failure(inputs_result.GetError());
    }

    const auto& inputs = inputs_result.Value();
    RuntimeSupervisionSnapshot snapshot;
    snapshot.connected = inputs.connected;
    snapshot.connection_state = inputs.connected ? "connected" : "disconnected";
    if (inputs.has_hardware_connection_port) {
        snapshot.connection_state = inputs.heartbeat_status.is_degraded
            ? "degraded"
            : ToConnectionStateLabel(inputs.connection_info.state);
    }
    snapshot.interlock_latched = inputs.interlock_latched;
    snapshot.active_job_id = inputs.active_job_id;
    snapshot.active_job_state = inputs.active_job_id.empty()
        ? std::string()
        : (inputs.active_job_status_available ? inputs.active_job_state : "unknown");
    snapshot.io = inputs.io;
    snapshot.effective_interlocks = BuildEffectiveInterlocksSnapshot(inputs);
    snapshot.supervision = BuildSupervisionSnapshot(inputs);
    return Result<RuntimeSupervisionSnapshot>::Success(std::move(snapshot));
}

}  // namespace Siligen::Runtime::Host::Supervision
