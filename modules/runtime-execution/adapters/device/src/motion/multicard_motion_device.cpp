#include "siligen/device/adapters/motion/multicard_motion_device.h"

#include "siligen/device/adapters/drivers/multicard/error_mapper.h"
#include "siligen/shared/error.h"

namespace Siligen::Device::Adapters::Motion {
namespace {

Siligen::SharedKernel::Error MakeHardwareError(Siligen::SharedKernel::ErrorCode code, const std::string& message) {
    return Siligen::SharedKernel::Error(code, message, "device-adapters.multicard");
}

short ToSdkAxisShort(Siligen::SharedKernel::LogicalAxisId axis) {
    return Siligen::SharedKernel::ToSdkShort(Siligen::SharedKernel::ToSdkAxis(axis));
}

long BuildAxisMask(const std::vector<Siligen::Device::Contracts::Commands::AxisTarget>& targets) {
    long mask = 0;
    for (const auto& target : targets) {
        const auto axis_index = Siligen::SharedKernel::ToIndex(target.axis);
        if (axis_index >= 0) {
            mask |= (1L << axis_index);
        }
    }
    return mask == 0 ? 1L : mask;
}

}  // namespace

MultiCardMotionDevice::MultiCardMotionDevice(
    std::shared_ptr<Siligen::Infrastructure::Hardware::IMultiCardWrapper> driver,
    Siligen::Device::Contracts::Capabilities::DeviceCapabilities capabilities)
    : driver_(std::move(driver)),
      capabilities_(std::move(capabilities)) {
    if (capabilities_.backend_name.empty()) {
        capabilities_.backend_name = "multicard";
    }
    if (capabilities_.axes.empty()) {
        capabilities_.axes = {
            {Siligen::SharedKernel::LogicalAxisId::X, true, true, true, true},
            {Siligen::SharedKernel::LogicalAxisId::Y, true, true, true, true},
        };
    }

    session_.backend_name = capabilities_.backend_name;
    session_.mock_backend = false;
    state_.axes.reserve(capabilities_.axes.size());
    for (const auto& axis_capability : capabilities_.axes) {
        Siligen::Device::Contracts::State::AxisState axis_state;
        axis_state.axis = axis_capability.axis;
        state_.axes.push_back(axis_state);
    }
}

Siligen::SharedKernel::VoidResult MultiCardMotionDevice::Connect(
    const Siligen::Device::Contracts::Commands::DeviceConnection& connection) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!driver_) {
        return Siligen::SharedKernel::VoidResult::Failure(
            MakeHardwareError(Siligen::SharedKernel::ErrorCode::ADAPTER_NOT_INITIALIZED, "MultiCard driver is null"));
    }
    if (connection.local_ip.empty() || connection.card_ip.empty()) {
        return Siligen::SharedKernel::VoidResult::Failure(
            MakeHardwareError(Siligen::SharedKernel::ErrorCode::INVALID_PARAMETER, "MultiCard IP config is required"));
    }

    session_.state = Siligen::Device::Contracts::State::DeviceSessionState::kConnecting;
    session_.backend_name = connection.backend_name.empty() ? capabilities_.backend_name : connection.backend_name;

    auto local_ip = connection.local_ip;
    auto card_ip = connection.card_ip;
    const int result = driver_->MC_Open(
        connection.card_number,
        local_ip.data(),
        connection.local_port,
        card_ip.data(),
        connection.card_port);
    if (result != 0) {
        ReplaceActiveFault(result, "MC_Open");
        session_.state = Siligen::Device::Contracts::State::DeviceSessionState::kFaulted;
        return Siligen::SharedKernel::VoidResult::Failure(
            MakeHardwareError(
                Siligen::SharedKernel::ErrorCode::HARDWARE_CONNECTION_FAILED,
                Siligen::Hal::Drivers::MultiCard::ErrorMapper::FormatErrorMessage(result, "MC_Open")));
    }

    ClearActiveFaults();
    session_.session_id = session_.backend_name + "#" + std::to_string(connection.card_number);
    session_.state = Siligen::Device::Contracts::State::DeviceSessionState::kReady;
    state_.connected = true;
    state_.estop_active = false;
    state_.lifecycle = Siligen::Device::Contracts::State::MotionLifecycleState::kIdle;
    return Siligen::SharedKernel::VoidResult::Success();
}

Siligen::SharedKernel::VoidResult MultiCardMotionDevice::Disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!driver_) {
        return Siligen::SharedKernel::VoidResult::Failure(
            MakeHardwareError(Siligen::SharedKernel::ErrorCode::ADAPTER_NOT_INITIALIZED, "MultiCard driver is null"));
    }

    const int result = driver_->MC_Close();
    if (result != 0) {
        ReplaceActiveFault(result, "MC_Close");
        session_.state = Siligen::Device::Contracts::State::DeviceSessionState::kFaulted;
        return Siligen::SharedKernel::VoidResult::Failure(
            MakeHardwareError(
                Siligen::SharedKernel::ErrorCode::HARDWARE_COMMAND_FAILED,
                Siligen::Hal::Drivers::MultiCard::ErrorMapper::FormatErrorMessage(result, "MC_Close")));
    }

    state_.connected = false;
    state_.lifecycle = Siligen::Device::Contracts::State::MotionLifecycleState::kStopped;
    session_.state = Siligen::Device::Contracts::State::DeviceSessionState::kDisconnected;
    return Siligen::SharedKernel::VoidResult::Success();
}

Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DeviceSession> MultiCardMotionDevice::ReadSession()
    const {
    std::lock_guard<std::mutex> lock(mutex_);
    return Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::DeviceSession>::Success(session_);
}

Siligen::SharedKernel::VoidResult MultiCardMotionDevice::Execute(
    const Siligen::Device::Contracts::Commands::MotionCommand& command) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!state_.connected) {
        return Siligen::SharedKernel::VoidResult::Failure(
            MakeHardwareError(Siligen::SharedKernel::ErrorCode::HARDWARE_NOT_CONNECTED, "MultiCard is not connected"));
    }

    switch (command.kind) {
        case Siligen::Device::Contracts::Commands::MotionCommandKind::kMoveTo:
            return ExecuteMove(command);
        case Siligen::Device::Contracts::Commands::MotionCommandKind::kJog:
            return ExecuteJog(command);
        case Siligen::Device::Contracts::Commands::MotionCommandKind::kHome:
            return ExecuteHome(command);
        case Siligen::Device::Contracts::Commands::MotionCommandKind::kStop:
            return Stop(false);
        case Siligen::Device::Contracts::Commands::MotionCommandKind::kEmergencyStop:
            return Stop(true);
        case Siligen::Device::Contracts::Commands::MotionCommandKind::kExecuteTrajectory:
            return Siligen::SharedKernel::VoidResult::Failure(
                MakeHardwareError(
                    Siligen::SharedKernel::ErrorCode::NOT_IMPLEMENTED,
                    "Trajectory execution will be split into an interpolation port in a follow-up task"));
        default:
            return Siligen::SharedKernel::VoidResult::Failure(
                MakeHardwareError(Siligen::SharedKernel::ErrorCode::INVALID_PARAMETER, "Unknown motion command"));
    }
}

Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::MotionState> MultiCardMotionDevice::ReadState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::MotionState>::Success(state_);
}

Siligen::SharedKernel::Result<Siligen::Device::Contracts::Capabilities::DeviceCapabilities>
MultiCardMotionDevice::DescribeCapabilities() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return Siligen::SharedKernel::Result<Siligen::Device::Contracts::Capabilities::DeviceCapabilities>::Success(
        capabilities_);
}

Siligen::SharedKernel::Result<std::vector<Siligen::Device::Contracts::Faults::DeviceFault>>
MultiCardMotionDevice::ReadFaults() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return Siligen::SharedKernel::Result<std::vector<Siligen::Device::Contracts::Faults::DeviceFault>>::Success(
        active_faults_);
}

Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::MachineHealthSnapshot>
MultiCardMotionDevice::ReadHealth() const {
    std::lock_guard<std::mutex> lock(mutex_);
    Siligen::Device::Contracts::State::MachineHealthSnapshot snapshot;
    snapshot.connected = state_.connected;
    snapshot.estop_active = state_.estop_active;
    snapshot.active_faults = active_faults_;
    return Siligen::SharedKernel::Result<Siligen::Device::Contracts::State::MachineHealthSnapshot>::Success(snapshot);
}

Siligen::SharedKernel::VoidResult MultiCardMotionDevice::ExecuteMove(
    const Siligen::Device::Contracts::Commands::MotionCommand& command) {
    long mask = 0;
    for (const auto& target : command.targets) {
        const auto sdk_axis = ToSdkAxisShort(target.axis);
        const int set_pos_result = driver_->MC_SetPos(sdk_axis, static_cast<long>(target.position_mm));
        if (set_pos_result != 0) {
            ReplaceActiveFault(set_pos_result, "MC_SetPos");
            return Siligen::SharedKernel::VoidResult::Failure(
                MakeHardwareError(
                    Siligen::SharedKernel::ErrorCode::HARDWARE_COMMAND_FAILED,
                    Siligen::Hal::Drivers::MultiCard::ErrorMapper::FormatErrorMessage(set_pos_result, "MC_SetPos")));
        }

        const int set_vel_result = driver_->MC_SetVel(sdk_axis, target.velocity_mm_s);
        if (set_vel_result != 0) {
            ReplaceActiveFault(set_vel_result, "MC_SetVel");
            return Siligen::SharedKernel::VoidResult::Failure(
                MakeHardwareError(
                    Siligen::SharedKernel::ErrorCode::HARDWARE_COMMAND_FAILED,
                    Siligen::Hal::Drivers::MultiCard::ErrorMapper::FormatErrorMessage(set_vel_result, "MC_SetVel")));
        }

        mask |= (1L << Siligen::SharedKernel::ToIndex(target.axis));
        for (auto& axis_state : state_.axes) {
            if (axis_state.axis == target.axis) {
                axis_state.position_mm = target.position_mm;
                axis_state.velocity_mm_s = target.velocity_mm_s;
                axis_state.servo_enabled = true;
            }
        }
    }

    const int update_result = driver_->MC_Update(mask == 0 ? 1L : mask);
    if (update_result != 0) {
        ReplaceActiveFault(update_result, "MC_Update");
        return Siligen::SharedKernel::VoidResult::Failure(
            MakeHardwareError(
                Siligen::SharedKernel::ErrorCode::HARDWARE_COMMAND_FAILED,
                Siligen::Hal::Drivers::MultiCard::ErrorMapper::FormatErrorMessage(update_result, "MC_Update")));
    }

    ClearActiveFaults();
    state_.correlation_id = command.correlation_id;
    state_.lifecycle = Siligen::Device::Contracts::State::MotionLifecycleState::kRunning;
    session_.state = Siligen::Device::Contracts::State::DeviceSessionState::kRunning;
    return Siligen::SharedKernel::VoidResult::Success();
}

Siligen::SharedKernel::VoidResult MultiCardMotionDevice::ExecuteHome(
    const Siligen::Device::Contracts::Commands::MotionCommand& command) {
    auto targets = command.targets;
    if (targets.empty()) {
        targets.push_back({Siligen::SharedKernel::LogicalAxisId::X, 0.0, 0.0});
        targets.push_back({Siligen::SharedKernel::LogicalAxisId::Y, 0.0, 0.0});
    }

    for (const auto& target : targets) {
        const int result = driver_->MC_HomeStart(ToSdkAxisShort(target.axis));
        if (result != 0) {
            ReplaceActiveFault(result, "MC_HomeStart");
            return Siligen::SharedKernel::VoidResult::Failure(
                MakeHardwareError(
                    Siligen::SharedKernel::ErrorCode::HARDWARE_COMMAND_FAILED,
                    Siligen::Hal::Drivers::MultiCard::ErrorMapper::FormatErrorMessage(result, "MC_HomeStart")));
        }

        for (auto& axis_state : state_.axes) {
            if (axis_state.axis == target.axis) {
                axis_state.homed = true;
                axis_state.position_mm = 0.0;
            }
        }
    }

    ClearActiveFaults();
    state_.correlation_id = command.correlation_id;
    state_.lifecycle = Siligen::Device::Contracts::State::MotionLifecycleState::kPrepared;
    return Siligen::SharedKernel::VoidResult::Success();
}

Siligen::SharedKernel::VoidResult MultiCardMotionDevice::ExecuteJog(
    const Siligen::Device::Contracts::Commands::MotionCommand& command) {
    for (const auto& target : command.targets) {
        const auto sdk_axis = ToSdkAxisShort(target.axis);
        const int jog_result = driver_->MC_PrfJog(sdk_axis);
        if (jog_result != 0) {
            ReplaceActiveFault(jog_result, "MC_PrfJog");
            return Siligen::SharedKernel::VoidResult::Failure(
                MakeHardwareError(
                    Siligen::SharedKernel::ErrorCode::HARDWARE_COMMAND_FAILED,
                    Siligen::Hal::Drivers::MultiCard::ErrorMapper::FormatErrorMessage(jog_result, "MC_PrfJog")));
        }

        const int velocity_result = driver_->MC_SetVel(sdk_axis, target.velocity_mm_s);
        if (velocity_result != 0) {
            ReplaceActiveFault(velocity_result, "MC_SetVel");
            return Siligen::SharedKernel::VoidResult::Failure(
                MakeHardwareError(
                    Siligen::SharedKernel::ErrorCode::HARDWARE_COMMAND_FAILED,
                    Siligen::Hal::Drivers::MultiCard::ErrorMapper::FormatErrorMessage(velocity_result, "MC_SetVel")));
        }
    }

    const int update_result = driver_->MC_Update(BuildAxisMask(command.targets));
    if (update_result != 0) {
        ReplaceActiveFault(update_result, "MC_Update");
        return Siligen::SharedKernel::VoidResult::Failure(
            MakeHardwareError(
                Siligen::SharedKernel::ErrorCode::HARDWARE_COMMAND_FAILED,
                Siligen::Hal::Drivers::MultiCard::ErrorMapper::FormatErrorMessage(update_result, "MC_Update")));
    }

    ClearActiveFaults();
    state_.correlation_id = command.correlation_id;
    state_.lifecycle = Siligen::Device::Contracts::State::MotionLifecycleState::kRunning;
    return Siligen::SharedKernel::VoidResult::Success();
}

Siligen::SharedKernel::VoidResult MultiCardMotionDevice::Stop(bool immediate) {
    const int result = driver_->MC_Stop(0, immediate ? 1 : 0);
    if (result != 0) {
        ReplaceActiveFault(result, immediate ? "MC_Stop(immediate)" : "MC_Stop");
        return Siligen::SharedKernel::VoidResult::Failure(
            MakeHardwareError(
                Siligen::SharedKernel::ErrorCode::HARDWARE_COMMAND_FAILED,
                Siligen::Hal::Drivers::MultiCard::ErrorMapper::FormatErrorMessage(result, "MC_Stop")));
    }

    ClearActiveFaults();
    state_.estop_active = immediate;
    state_.lifecycle = immediate ? Siligen::Device::Contracts::State::MotionLifecycleState::kFaulted
                                 : Siligen::Device::Contracts::State::MotionLifecycleState::kStopped;
    session_.state = immediate ? Siligen::Device::Contracts::State::DeviceSessionState::kFaulted
                               : Siligen::Device::Contracts::State::DeviceSessionState::kReady;
    return Siligen::SharedKernel::VoidResult::Success();
}

void MultiCardMotionDevice::ReplaceActiveFault(int error_code, const std::string& operation) {
    const auto& info = Siligen::Hal::Drivers::MultiCard::ErrorMapper::GetErrorInfo(error_code);
    Siligen::Device::Contracts::Faults::DeviceFault fault;
    fault.code = info.name.empty() ? std::to_string(error_code) : info.name;
    fault.message = info.description.empty() ? operation : info.description;
    fault.hint = info.hint;
    fault.category = Siligen::Device::Contracts::Faults::DeviceFaultCategory::kVendorSdk;
    fault.severity = info.severity == "CRITICAL"
                         ? Siligen::Device::Contracts::Faults::DeviceFaultSeverity::kCritical
                         : info.severity == "WARNING"
                               ? Siligen::Device::Contracts::Faults::DeviceFaultSeverity::kWarning
                               : info.severity == "INFO"
                                     ? Siligen::Device::Contracts::Faults::DeviceFaultSeverity::kInfo
                                     : Siligen::Device::Contracts::Faults::DeviceFaultSeverity::kError;
    active_faults_.clear();
    active_faults_.push_back(fault);
    state_.fault_message = Siligen::Hal::Drivers::MultiCard::ErrorMapper::FormatErrorMessage(error_code, operation);
}

void MultiCardMotionDevice::ClearActiveFaults() {
    active_faults_.clear();
    state_.fault_message.clear();
}

}  // namespace Siligen::Device::Adapters::Motion
