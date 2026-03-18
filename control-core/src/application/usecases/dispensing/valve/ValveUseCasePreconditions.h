#pragma once

#include "domain/machine/ports/IHardwareConnectionPort.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <memory>

namespace Siligen::Application::UseCases::Dispensing::Valve::Preconditions {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

inline Result<void> EnsureHardwareConnected(
    const std::shared_ptr<Domain::Machine::Ports::IHardwareConnectionPort>& connection_port) {
    if (!connection_port) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Hardware connection port not initialized"));
    }

    auto conn_info = connection_port->GetConnectionInfo();
    if (conn_info.state != Domain::Machine::Ports::HardwareConnectionState::Connected) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "硬件未连接，请先连接硬件"));
    }

    return Result<void>::Success();
}

}  // namespace Siligen::Application::UseCases::Dispensing::Valve::Preconditions





