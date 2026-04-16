#pragma once

#include "siligen/shared/axis_types.h"
#include "siligen/shared/numeric_types.h"

#include <string>
#include <vector>

namespace Siligen::Device::Contracts::Capabilities {

struct AxisCapability {
    Siligen::SharedKernel::LogicalAxisId axis = Siligen::SharedKernel::LogicalAxisId::X;
    bool supports_home = true;
    bool supports_jog = true;
    bool supports_absolute_move = true;
    bool supports_feedback = true;
};

struct DigitalIoCapability {
    Siligen::SharedKernel::int32 input_channels = 0;
    Siligen::SharedKernel::int32 output_channels = 0;
    bool supports_latched_output = false;
};

struct TriggerCapability {
    bool supports_position_trigger = false;
    bool supports_time_trigger = false;
    bool supports_in_motion_position_trigger = false;
    bool supports_in_motion_time_trigger = false;
};

struct DispenserCapability {
    bool supports_prime = true;
    bool supports_pause = true;
    bool supports_resume = true;
    bool supports_continuous_mode = false;
    bool supports_in_motion_pulse_shot = false;
};

struct DeviceCapabilities {
    std::string backend_name;
    bool mock_backend = false;
    std::vector<AxisCapability> axes;
    DigitalIoCapability io;
    TriggerCapability trigger;
    DispenserCapability dispenser;
};

}  // namespace Siligen::Device::Contracts::Capabilities
