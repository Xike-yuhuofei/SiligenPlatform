#pragma once

#include "siligen/device/contracts/faults/device_faults.h"
#include "siligen/shared/axis_types.h"
#include "siligen/shared/numeric_types.h"

#include <cstdint>
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

enum class DeviceConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Error,
};

struct DeviceConnectionSnapshot {
    DeviceConnectionState state = DeviceConnectionState::Disconnected;
    std::string device_type = "MultiCard";
    std::string firmware_version;
    std::string serial_number;
    std::string error_message;

    bool IsConnected() const {
        return state == DeviceConnectionState::Connected;
    }

    bool HasError() const {
        return state == DeviceConnectionState::Error || !error_message.empty();
    }
};

struct HeartbeatSnapshot {
    bool enabled = true;
    std::uint32_t interval_ms = 3000;
    std::uint32_t timeout_ms = 5000;
    std::uint32_t failure_threshold = 3;
    bool is_active = false;
    std::uint64_t total_sent = 0;
    std::uint64_t total_success = 0;
    std::uint64_t total_failure = 0;
    std::uint32_t consecutive_failures = 0;
    std::string last_error;
    bool is_degraded = false;
    std::string degraded_reason;

    bool IsValidConfig() const noexcept {
        return interval_ms >= 100 && interval_ms <= 60000 && timeout_ms >= interval_ms &&
               failure_threshold >= 1 && failure_threshold <= 10;
    }
};

struct DeviceSession {
    std::string session_id;
    DeviceSessionState state = DeviceSessionState::kDisconnected;
    bool mock_backend = false;
    std::string backend_name;
};

}  // namespace Siligen::Device::Contracts::State
