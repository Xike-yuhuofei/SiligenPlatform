#pragma once

#include "siligen/device/contracts/faults/device_faults.h"
#include "siligen/shared/axis_types.h"
#include "siligen/shared/numeric_types.h"

#include <string>
#include <vector>

namespace Siligen::Device::Contracts::State {

enum class DeviceSessionState {
    kDisconnected,
    kConnecting,
    kReady,
    kRunning,
    kFaulted,
};

enum class MotionLifecycleState {
    kIdle,
    kPrepared,
    kRunning,
    kPaused,
    kCompleted,
    kStopped,
    kFaulted,
};

struct AxisState {
    Siligen::SharedKernel::LogicalAxisId axis = Siligen::SharedKernel::LogicalAxisId::X;
    double position_mm = 0.0;
    double velocity_mm_s = 0.0;
    bool servo_enabled = false;
    bool homed = false;
};

struct MotionState {
    MotionLifecycleState lifecycle = MotionLifecycleState::kIdle;
    bool estop_active = false;
    bool connected = false;
    std::vector<AxisState> axes;
    std::string correlation_id;
    std::string fault_message;
};

struct DigitalIoState {
    Siligen::SharedKernel::int32 channel = 0;
    bool value = false;
    bool output = false;
};

struct DispenserState {
    bool primed = false;
    bool running = false;
    bool paused = false;
    std::string execution_id;
};

struct MachineHealthSnapshot {
    bool connected = false;
    bool estop_active = false;
    std::vector<Siligen::Device::Contracts::Faults::DeviceFault> active_faults;
};

struct DeviceSession {
    std::string session_id;
    DeviceSessionState state = DeviceSessionState::kDisconnected;
    bool mock_backend = false;
    std::string backend_name;
};

}  // namespace Siligen::Device::Contracts::State
