#include "MotionInitializationUseCase.h"

#include "shared/types/Error.h"

#include <utility>

using namespace Siligen::Shared::Types;

namespace Siligen::Application::UseCases::Motion::Initialization {

MotionInitializationUseCase::MotionInitializationUseCase(
    std::shared_ptr<Domain::Motion::Ports::IMotionConnectionPort> motion_connection_port,
    std::shared_ptr<Domain::Motion::Ports::IAxisControlPort> axis_control_port,
    std::shared_ptr<Siligen::RuntimeExecution::Contracts::Motion::IIOControlPort> io_port)
    : motion_connection_port_(std::move(motion_connection_port))
    , axis_control_port_(std::move(axis_control_port))
    , io_port_(std::move(io_port)) {
}

Result<void> MotionInitializationUseCase::ConnectToController(const std::string& card_ip,
                                                              const std::string& pc_ip,
                                                              int16 port) {
    if (!motion_connection_port_) {
        return Result<void>::Success();
    }
    auto result = motion_connection_port_->Connect(card_ip, pc_ip, port);
    if (result.IsError() && result.GetError().GetCode() == ErrorCode::NOT_IMPLEMENTED) {
        return Result<void>::Success();
    }
    return result;
}

Result<void> MotionInitializationUseCase::DisconnectFromController() {
    if (!motion_connection_port_) {
        return Result<void>::Success();
    }
    auto result = motion_connection_port_->Disconnect();
    if (result.IsError() && result.GetError().GetCode() == ErrorCode::NOT_IMPLEMENTED) {
        return Result<void>::Success();
    }
    return result;
}

Result<void> MotionInitializationUseCase::ResetController() {
    if (!motion_connection_port_) {
        return Result<void>::Success();
    }
    auto result = motion_connection_port_->Reset();
    if (result.IsError() && result.GetError().GetCode() == ErrorCode::NOT_IMPLEMENTED) {
        return Result<void>::Success();
    }
    return result;
}

Result<bool> MotionInitializationUseCase::IsControllerConnected() const {
    if (motion_connection_port_) {
        auto result = motion_connection_port_->IsConnected();
        if (result.IsSuccess()) {
            return Result<bool>::Success(result.Value());
        }
        return Result<bool>::Failure(result.GetError());
    }

    return Result<bool>::Success(false);
}

Result<void> MotionInitializationUseCase::EnableAxis(LogicalAxisId axis) {
    if (!axis_control_port_) {
        return Result<void>::Success();
    }
    auto result = axis_control_port_->EnableAxis(axis);
    if (result.IsError() && result.GetError().GetCode() == ErrorCode::NOT_IMPLEMENTED) {
        return Result<void>::Success();
    }
    return result;
}

Result<void> MotionInitializationUseCase::DisableAxis(LogicalAxisId axis) {
    if (!axis_control_port_) {
        return Result<void>::Success();
    }
    auto result = axis_control_port_->DisableAxis(axis);
    if (result.IsError() && result.GetError().GetCode() == ErrorCode::NOT_IMPLEMENTED) {
        return Result<void>::Success();
    }
    return result;
}

Result<void> MotionInitializationUseCase::ClearAxisPosition(LogicalAxisId axis) {
    if (!axis_control_port_) {
        return Result<void>::Success();
    }
    auto result = axis_control_port_->ClearPosition(axis);
    if (result.IsError() && result.GetError().GetCode() == ErrorCode::NOT_IMPLEMENTED) {
        return Result<void>::Success();
    }
    return result;
}

Result<void> MotionInitializationUseCase::ClearAxisStatus(LogicalAxisId axis) {
    if (!axis_control_port_) {
        return Result<void>::Success();
    }
    auto result = axis_control_port_->ClearAxisStatus(axis);
    if (result.IsError() && result.GetError().GetCode() == ErrorCode::NOT_IMPLEMENTED) {
        return Result<void>::Success();
    }
    return result;
}

}  // namespace Siligen::Application::UseCases::Motion::Initialization


