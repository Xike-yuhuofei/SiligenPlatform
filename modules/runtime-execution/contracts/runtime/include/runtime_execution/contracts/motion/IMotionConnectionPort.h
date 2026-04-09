#pragma once

#include "shared/types/Result.h"

#include <cstdint>
#include <string>

using Siligen::Shared::Types::Result;

using int16 = std::int16_t;

namespace Siligen::RuntimeExecution::Contracts::Motion {

class IMotionConnectionPort {
   public:
    virtual ~IMotionConnectionPort() = default;

    virtual Result<void> Connect(const std::string& card_ip, const std::string& pc_ip, int16 port = 0) = 0;
    virtual Result<void> Disconnect() = 0;
    virtual Result<bool> IsConnected() const = 0;
    virtual Result<void> Reset() = 0;
    virtual Result<std::string> GetCardInfo() const = 0;
};

}  // namespace Siligen::RuntimeExecution::Contracts::Motion

namespace Siligen::Domain::Motion::Ports {

using IMotionConnectionPort = Siligen::RuntimeExecution::Contracts::Motion::IMotionConnectionPort;

}  // namespace Siligen::Domain::Motion::Ports
