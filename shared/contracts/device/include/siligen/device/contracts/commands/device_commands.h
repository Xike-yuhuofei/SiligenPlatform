#pragma once

#include "siligen/shared/axis_types.h"
#include "siligen/shared/numeric_types.h"

#include <string>
#include <vector>

namespace Siligen::Device::Contracts::Commands {

enum class MotionCommandKind {
    kJog,
    kMoveTo,
    kHome,
    kStop,
    kEmergencyStop,
    kExecuteTrajectory,
};

struct AxisTarget {
    Siligen::SharedKernel::LogicalAxisId axis = Siligen::SharedKernel::LogicalAxisId::X;
    double position_mm = 0.0;
    double velocity_mm_s = 0.0;
};

struct MotionCommand {
    MotionCommandKind kind = MotionCommandKind::kMoveTo;
    std::vector<AxisTarget> targets;
    bool relative = false;
    std::string correlation_id;
};

enum class DispenserCommandKind {
    kPrime,
    kStart,
    kPause,
    kResume,
    kStop,
};

struct DispenserCommand {
    DispenserCommandKind kind = DispenserCommandKind::kStart;
    std::string execution_id;
    double requested_duration_ms = 0.0;
};

enum class DigitalIoCommandKind {
    kReadInput,
    kReadOutput,
    kWriteOutput,
};

struct DigitalIoCommand {
    DigitalIoCommandKind kind = DigitalIoCommandKind::kReadInput;
    Siligen::SharedKernel::int32 channel = 0;
    bool value = false;
};

struct DeviceConnection {
    short card_number = 1;
    std::string local_ip;
    unsigned short local_port = 0;
    std::string card_ip;
    unsigned short card_port = 0;
    std::string backend_name;
};

}  // namespace Siligen::Device::Contracts::Commands
