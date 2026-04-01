#pragma once

#include "runtime_execution/contracts/system/IRuntimeSupervisionPort.h"
#include "siligen/device/contracts/state/device_state.h"

#include <memory>

namespace Siligen::Runtime::Host::Supervision {

struct RuntimeSupervisionInputs {
    bool has_hardware_connection_port = false;
    Siligen::Device::Contracts::State::DeviceConnectionSnapshot connection_info{};
    Siligen::Device::Contracts::State::HeartbeatSnapshot heartbeat_status{};

    bool connected = false;
    bool estop_state_known = false;
    bool estop_active = false;
    bool any_axis_fault = false;
    bool any_axis_moving = false;
    bool home_boundary_x_active = false;
    bool home_boundary_y_active = false;

    Siligen::RuntimeExecution::Contracts::System::RawIoStatus io{};
    bool interlock_latched = false;

    std::string active_job_id;
    bool active_job_status_available = false;
    std::string active_job_state;
};

class IRuntimeSupervisionBackend {
   public:
    virtual ~IRuntimeSupervisionBackend() = default;

    virtual Siligen::Shared::Types::Result<RuntimeSupervisionInputs> ReadInputs() const = 0;
};

class RuntimeSupervisionPortAdapter final
    : public Siligen::RuntimeExecution::Contracts::System::IRuntimeSupervisionPort {
   public:
    explicit RuntimeSupervisionPortAdapter(std::shared_ptr<IRuntimeSupervisionBackend> backend);

    Siligen::Shared::Types::Result<Siligen::RuntimeExecution::Contracts::System::RuntimeSupervisionSnapshot>
    ReadSnapshot() const override;

   private:
    std::shared_ptr<IRuntimeSupervisionBackend> backend_;
};

}  // namespace Siligen::Runtime::Host::Supervision
