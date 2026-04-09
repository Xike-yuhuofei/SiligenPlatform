#pragma once

#include "shared/types/Result.h"
#include "shared/types/Types.h"

#include <cstdint>

using Siligen::Shared::Types::int32;
using Siligen::Shared::Types::LogicalAxisId;
using Siligen::Shared::Types::Result;

using int16 = std::int16_t;
using uint32 = std::uint32_t;
using int64 = std::int64_t;

namespace Siligen::RuntimeExecution::Contracts::Motion {

/**
 * @brief M9 owner 的 IO 状态快照。
 */
struct IOStatus {
    bool signal_active = false;
    int32 channel = 0;
    uint32 value = 0;
    int64 timestamp = 0;
};

/**
 * @brief M9 owner 的 IO 控制边界。
 */
class IIOControlPort {
   public:
    virtual ~IIOControlPort() = default;

    virtual Result<IOStatus> ReadDigitalInput(int16 channel) = 0;
    virtual Result<IOStatus> ReadDigitalOutput(int16 channel) = 0;
    virtual Result<void> WriteDigitalOutput(int16 channel, bool value) = 0;
    virtual Result<bool> ReadLimitStatus(LogicalAxisId axis, bool positive) = 0;
    virtual Result<bool> ReadServoAlarm(LogicalAxisId axis) = 0;
};

}  // namespace Siligen::RuntimeExecution::Contracts::Motion

namespace Siligen::Domain::Motion::Ports {

using IOStatus = Siligen::RuntimeExecution::Contracts::Motion::IOStatus;
using IIOControlPort = Siligen::RuntimeExecution::Contracts::Motion::IIOControlPort;

}  // namespace Siligen::Domain::Motion::Ports
