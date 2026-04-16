#define MODULE_NAME "MotionRuntimeDevicePortAdapter"

#include "siligen/device/adapters/motion/MotionRuntimeDevicePortAdapter.h"

#include "siligen/device/adapters/motion/MotionRuntimeFacade.h"
#include "shared/interfaces/ILoggingService.h"

#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using DeviceCapabilities = Siligen::Device::Contracts::Capabilities::DeviceCapabilities;
using DeviceConnection = Siligen::Device::Contracts::Commands::DeviceConnection;
using DeviceConnectionSnapshot = Siligen::Device::Contracts::State::DeviceConnectionSnapshot;
using DeviceConnectionState = Siligen::Device::Contracts::State::DeviceConnectionState;
using DeviceFault = Siligen::Device::Contracts::Faults::DeviceFault;
using DeviceFaultCategory = Siligen::Device::Contracts::Faults::DeviceFaultCategory;
using DeviceFaultSeverity = Siligen::Device::Contracts::Faults::DeviceFaultSeverity;
using DeviceSession = Siligen::Device::Contracts::State::DeviceSession;
using DeviceSessionState = Siligen::Device::Contracts::State::DeviceSessionState;
using MotionCommand = Siligen::Device::Contracts::Commands::MotionCommand;
using MotionCommandKind = Siligen::Device::Contracts::Commands::MotionCommandKind;
using MotionLifecycleState = Siligen::Device::Contracts::State::MotionLifecycleState;
using MotionState = Siligen::Device::Contracts::State::MotionState;
using RuntimeMotionCommand = Siligen::Domain::Motion::Ports::MotionCommand;
using RuntimeMotionState = Siligen::Domain::Motion::Ports::MotionState;
using RuntimeMotionStatus = Siligen::Domain::Motion::Ports::MotionStatus;
using SharedError = Siligen::Shared::Types::Error;
using SharedKernelError = Siligen::SharedKernel::Error;
using SharedKernelErrorCode = Siligen::SharedKernel::ErrorCode;

SharedKernelError ToKernelError(const SharedError& error) {
    return SharedKernelError(
        static_cast<SharedKernelErrorCode>(static_cast<std::int32_t>(error.GetCode())),
        error.GetMessage(),
        error.GetModule());
}

Siligen::SharedKernel::VoidResult ToKernelVoidResult(const Siligen::Shared::Types::Result<void>& result) {
    if (result.IsError()) {
        return Siligen::SharedKernel::VoidResult::Failure(ToKernelError(result.GetError()));
    }
    return Siligen::SharedKernel::VoidResult::Success();
}

Siligen::Shared::Types::LogicalAxisId ToRuntimeAxis(Siligen::SharedKernel::LogicalAxisId axis) {
    return static_cast<Siligen::Shared::Types::LogicalAxisId>(static_cast<std::int16_t>(axis));
}

DeviceFault MakeConnectivityFault(const std::string& message, DeviceFaultSeverity severity) {
    DeviceFault fault;
    fault.code = "motion-runtime-connectivity";
    fault.severity = severity;
    fault.category = DeviceFaultCategory::kConnectivity;
    fault.message = message;
    fault.hint = "检查 runtime-service 设备连接和 MultiCard 通讯状态";
    fault.active = true;
    return fault;
}

MotionLifecycleState ResolveLifecycle(
    const std::vector<RuntimeMotionStatus>& statuses,
    bool connected,
    bool estop_active,
    bool has_fault_message) {
    if (!connected) {
        return has_fault_message ? MotionLifecycleState::kFaulted : MotionLifecycleState::kStopped;
    }
    if (estop_active || has_fault_message) {
        return MotionLifecycleState::kFaulted;
    }

    bool saw_homed = false;
    bool saw_stopped = false;
    for (const auto& status : statuses) {
        if (status.state == RuntimeMotionState::FAULT) {
            return MotionLifecycleState::kFaulted;
        }
        if (status.state == RuntimeMotionState::ESTOP) {
            return MotionLifecycleState::kFaulted;
        }
        if (status.state == RuntimeMotionState::MOVING || status.state == RuntimeMotionState::HOMING) {
            return MotionLifecycleState::kRunning;
        }
        if (status.state == RuntimeMotionState::STOPPED) {
            saw_stopped = true;
        }
        if (status.state == RuntimeMotionState::HOMED) {
            saw_homed = true;
        }
    }

    if (saw_stopped) {
        return MotionLifecycleState::kStopped;
    }
    if (saw_homed) {
        return MotionLifecycleState::kPrepared;
    }
    return MotionLifecycleState::kIdle;
}

DeviceSessionState ResolveSessionState(
    const DeviceConnectionSnapshot& snapshot,
    MotionLifecycleState lifecycle) {
    switch (snapshot.state) {
        case DeviceConnectionState::Disconnected:
            return DeviceSessionState::kDisconnected;
        case DeviceConnectionState::Connecting:
            return DeviceSessionState::kConnecting;
        case DeviceConnectionState::Error:
            return DeviceSessionState::kFaulted;
        case DeviceConnectionState::Connected:
            return lifecycle == MotionLifecycleState::kRunning
                ? DeviceSessionState::kRunning
                : lifecycle == MotionLifecycleState::kFaulted
                    ? DeviceSessionState::kFaulted
                    : DeviceSessionState::kReady;
    }

    return DeviceSessionState::kFaulted;
}

std::vector<RuntimeMotionCommand> BuildRuntimeCommands(
    const std::vector<Siligen::Device::Contracts::Commands::AxisTarget>& targets,
    bool relative) {
    std::vector<RuntimeMotionCommand> runtime_commands;
    runtime_commands.reserve(targets.size());
    for (const auto& target : targets) {
        RuntimeMotionCommand runtime_command;
        runtime_command.axis = ToRuntimeAxis(target.axis);
        runtime_command.position = static_cast<Siligen::Shared::Types::float32>(target.position_mm);
        runtime_command.velocity = static_cast<Siligen::Shared::Types::float32>(target.velocity_mm_s);
        runtime_command.relative = relative;
        runtime_commands.push_back(runtime_command);
    }
    return runtime_commands;
}

}  // namespace

namespace Siligen::Infrastructure::Adapters::Motion {

MotionRuntimeDevicePortAdapter::MotionRuntimeDevicePortAdapter(
    std::shared_ptr<MotionRuntimeFacade> runtime,
    DeviceCapabilities capabilities)
    : runtime_(std::move(runtime)),
      capabilities_(std::move(capabilities)) {
    if (!runtime_) {
        throw std::invalid_argument("MotionRuntimeDevicePortAdapter: runtime cannot be null");
    }
}

Siligen::SharedKernel::VoidResult MotionRuntimeDevicePortAdapter::Connect(const DeviceConnection& connection) {
    return ToKernelVoidResult(runtime_->ConnectDevice(connection));
}

Siligen::SharedKernel::VoidResult MotionRuntimeDevicePortAdapter::Disconnect() {
    return ToKernelVoidResult(runtime_->Disconnect());
}

Siligen::SharedKernel::Result<DeviceSession> MotionRuntimeDevicePortAdapter::ReadSession() const {
    const auto connection_result = runtime_->ReadConnection();
    if (connection_result.IsError()) {
        return Siligen::SharedKernel::Result<DeviceSession>::Failure(ToKernelError(connection_result.GetError()));
    }

    const auto state_result = ReadState();
    if (state_result.IsError()) {
        return Siligen::SharedKernel::Result<DeviceSession>::Failure(state_result.GetError());
    }

    DeviceSession session;
    session.backend_name = capabilities_.backend_name;
    session.mock_backend = capabilities_.mock_backend;
    session.session_id = capabilities_.backend_name.empty()
        ? std::string("motion-runtime")
        : capabilities_.backend_name + "#runtime";
    session.state = ResolveSessionState(connection_result.Value(), state_result.Value().lifecycle);
    return Siligen::SharedKernel::Result<DeviceSession>::Success(session);
}

Siligen::SharedKernel::VoidResult MotionRuntimeDevicePortAdapter::Execute(const MotionCommand& command) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        last_correlation_id_ = command.correlation_id;
    }

    switch (command.kind) {
        case MotionCommandKind::kMoveTo: {
            if (command.targets.empty()) {
                return Siligen::SharedKernel::VoidResult::Failure(SharedKernelError(
                    SharedKernelErrorCode::INVALID_PARAMETER,
                    "motion targets cannot be empty",
                    "MotionRuntimeDevicePortAdapter"));
            }
            return ToKernelVoidResult(runtime_->SynchronizedMove(BuildRuntimeCommands(command.targets, command.relative)));
        }
        case MotionCommandKind::kHome: {
            if (command.targets.empty() && capabilities_.axes.empty()) {
                return Siligen::SharedKernel::VoidResult::Failure(SharedKernelError(
                    SharedKernelErrorCode::INVALID_STATE,
                    "motion device has no configured axes to home",
                    "MotionRuntimeDevicePortAdapter"));
            }

            if (command.targets.empty()) {
                for (const auto& axis_capability : capabilities_.axes) {
                    const auto result = runtime_->HomeAxis(ToRuntimeAxis(axis_capability.axis));
                    if (result.IsError()) {
                        return Siligen::SharedKernel::VoidResult::Failure(ToKernelError(result.GetError()));
                    }
                }
                return Siligen::SharedKernel::VoidResult::Success();
            }

            for (const auto& target : command.targets) {
                const auto result = runtime_->HomeAxis(ToRuntimeAxis(target.axis));
                if (result.IsError()) {
                    return Siligen::SharedKernel::VoidResult::Failure(ToKernelError(result.GetError()));
                }
            }
            return Siligen::SharedKernel::VoidResult::Success();
        }
        case MotionCommandKind::kStop:
            return ToKernelVoidResult(runtime_->StopAllAxes(false));
        case MotionCommandKind::kEmergencyStop:
            return ToKernelVoidResult(runtime_->EmergencyStop());
        case MotionCommandKind::kJog:
            return Siligen::SharedKernel::VoidResult::Failure(SharedKernelError(
                SharedKernelErrorCode::NOT_IMPLEMENTED,
                "MotionDevicePort::kJog is not expressible from runtime facade without additional jog contract",
                "MotionRuntimeDevicePortAdapter"));
        case MotionCommandKind::kExecuteTrajectory:
            return Siligen::SharedKernel::VoidResult::Failure(SharedKernelError(
                SharedKernelErrorCode::NOT_IMPLEMENTED,
                "MotionDevicePort::kExecuteTrajectory is owned by interpolation runtime, not MotionDevicePort",
                "MotionRuntimeDevicePortAdapter"));
    }

    return Siligen::SharedKernel::VoidResult::Failure(SharedKernelError(
        SharedKernelErrorCode::INVALID_PARAMETER,
        "unsupported motion command kind",
        "MotionRuntimeDevicePortAdapter"));
}

Siligen::SharedKernel::Result<MotionState> MotionRuntimeDevicePortAdapter::ReadState() const {
    const auto connection_result = runtime_->ReadConnection();
    if (connection_result.IsError()) {
        return Siligen::SharedKernel::Result<MotionState>::Failure(ToKernelError(connection_result.GetError()));
    }

    MotionState state;
    state.connected = connection_result.Value().IsConnected();
    state.fault_message = connection_result.Value().error_message;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state.correlation_id = last_correlation_id_;
    }

    if (!state.connected) {
        state.lifecycle = connection_result.Value().state == DeviceConnectionState::Error
            ? MotionLifecycleState::kFaulted
            : MotionLifecycleState::kStopped;
        return Siligen::SharedKernel::Result<MotionState>::Success(state);
    }

    const auto all_statuses_result = runtime_->GetAllAxesStatus();
    if (all_statuses_result.IsError()) {
        return Siligen::SharedKernel::Result<MotionState>::Failure(ToKernelError(all_statuses_result.GetError()));
    }

    const auto& all_statuses = all_statuses_result.Value();
    for (const auto& axis_capability : capabilities_.axes) {
        const auto runtime_axis = ToRuntimeAxis(axis_capability.axis);
        const auto axis_status_result = runtime_->GetAxisStatus(runtime_axis);
        if (axis_status_result.IsError()) {
            return Siligen::SharedKernel::Result<MotionState>::Failure(ToKernelError(axis_status_result.GetError()));
        }
        const auto axis_position_result = runtime_->GetAxisPosition(runtime_axis);
        if (axis_position_result.IsError()) {
            return Siligen::SharedKernel::Result<MotionState>::Failure(ToKernelError(axis_position_result.GetError()));
        }
        const auto axis_velocity_result = runtime_->GetAxisVelocity(runtime_axis);
        if (axis_velocity_result.IsError()) {
            return Siligen::SharedKernel::Result<MotionState>::Failure(ToKernelError(axis_velocity_result.GetError()));
        }
        const auto axis_homed_result = runtime_->IsAxisHomed(runtime_axis);
        if (axis_homed_result.IsError()) {
            return Siligen::SharedKernel::Result<MotionState>::Failure(ToKernelError(axis_homed_result.GetError()));
        }

        const auto& axis_status = axis_status_result.Value();
        Siligen::Device::Contracts::State::AxisState axis_state;
        axis_state.axis = axis_capability.axis;
        axis_state.position_mm = axis_position_result.Value();
        axis_state.velocity_mm_s = axis_velocity_result.Value();
        axis_state.servo_enabled = axis_status.enabled;
        axis_state.homed = axis_homed_result.Value();
        state.axes.push_back(axis_state);

        if (axis_status.state == RuntimeMotionState::ESTOP) {
            state.estop_active = true;
        }
    }

    state.lifecycle = ResolveLifecycle(
        all_statuses,
        state.connected,
        state.estop_active,
        !state.fault_message.empty());
    return Siligen::SharedKernel::Result<MotionState>::Success(state);
}

Siligen::SharedKernel::Result<DeviceCapabilities> MotionRuntimeDevicePortAdapter::DescribeCapabilities() const {
    return Siligen::SharedKernel::Result<DeviceCapabilities>::Success(capabilities_);
}

Siligen::SharedKernel::Result<std::vector<DeviceFault>> MotionRuntimeDevicePortAdapter::ReadFaults() const {
    const auto connection_result = runtime_->ReadConnection();
    if (connection_result.IsError()) {
        return Siligen::SharedKernel::Result<std::vector<DeviceFault>>::Failure(
            ToKernelError(connection_result.GetError()));
    }

    const auto& snapshot = connection_result.Value();
    if (!snapshot.IsConnected()) {
        if (snapshot.error_message.empty()) {
            return Siligen::SharedKernel::Result<std::vector<DeviceFault>>::Failure(SharedKernelError(
                SharedKernelErrorCode::HARDWARE_NOT_CONNECTED,
                "motion runtime is not connected",
                "MotionRuntimeDevicePortAdapter"));
        }
        return Siligen::SharedKernel::Result<std::vector<DeviceFault>>::Success(
            {MakeConnectivityFault(snapshot.error_message, DeviceFaultSeverity::kError)});
    }

    if (!snapshot.error_message.empty()) {
        return Siligen::SharedKernel::Result<std::vector<DeviceFault>>::Success(
            {MakeConnectivityFault(snapshot.error_message, DeviceFaultSeverity::kWarning)});
    }

    return Siligen::SharedKernel::Result<std::vector<DeviceFault>>::Success({});
}

}  // namespace Siligen::Infrastructure::Adapters::Motion
