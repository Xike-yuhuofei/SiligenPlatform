#pragma once

#include "siligen/device/contracts/ports/device_ports.h"
#include "shared/types/Error.h"
#include "shared/types/Result.h"

#include <memory>

namespace Siligen::Application::UseCases::Dispensing::Valve::Preconditions {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

inline Result<void> EnsureHardwareConnected(
    const std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort>& connection_port) {
    if (!connection_port) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "Hardware connection port not initialized"));
    }

    auto conn_info = connection_port->ReadConnection();
    if (conn_info.IsError()) {
        return Result<void>::Failure(conn_info.GetError());
    }
    if (!conn_info.Value().IsConnected()) {
        return Result<void>::Failure(
            Error(ErrorCode::HARDWARE_NOT_CONNECTED, "硬件未连接，请先连接硬件"));
    }

    return Result<void>::Success();
}

}  // namespace Siligen::Application::UseCases::Dispensing::Valve::Preconditions





