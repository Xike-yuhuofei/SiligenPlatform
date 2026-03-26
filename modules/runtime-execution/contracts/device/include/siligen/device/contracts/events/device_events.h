#pragma once

#include "siligen/device/contracts/faults/device_faults.h"
#include "siligen/device/contracts/state/device_state.h"

#include <chrono>
#include <string>

namespace Siligen::Device::Contracts::Events {

enum class DeviceEventKind {
    kSessionStateChanged,
    kMotionStateChanged,
    kDigitalIoChanged,
    kDispenserStateChanged,
    kFaultRaised,
    kFaultCleared,
    kHealthSnapshotUpdated,
};

struct DeviceEvent {
    DeviceEventKind kind = DeviceEventKind::kSessionStateChanged;
    std::string device_id;
    std::string correlation_id;
    std::string summary;
    std::chrono::system_clock::time_point occurred_at = std::chrono::system_clock::now();
};

struct FaultEvent {
    DeviceEvent base;
    Siligen::Device::Contracts::Faults::DeviceFault fault;
};

struct SessionEvent {
    DeviceEvent base;
    Siligen::Device::Contracts::State::DeviceSession session;
};

}  // namespace Siligen::Device::Contracts::Events
