#include "dispense_packaging/application/usecases/dispensing/valve/ValveCommandUseCase.h"
#include "dispense_packaging/application/usecases/dispensing/valve/ValveQueryUseCase.h"

#include "ValveUseCasePreconditions.h"
#include "shared/types/Error.h"

#include <utility>

namespace Siligen::Application::UseCases::Dispensing::Valve {

using Siligen::Shared::Types::Error;
using Siligen::Shared::Types::ErrorCode;
using Siligen::Shared::Types::Result;

namespace {

Result<void> EnsureValvePortAvailable(
    const std::shared_ptr<Domain::Dispensing::Ports::IValvePort>& valve_port) {
    if (!valve_port) {
        return Result<void>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "阀门端口未初始化"));
    }

    return Result<void>::Success();
}

}  // namespace

ValveCommandUseCase::ValveCommandUseCase(
    std::shared_ptr<Domain::Dispensing::DomainServices::ValveCoordinationService> valve_service,
    std::shared_ptr<Domain::Dispensing::Ports::IValvePort> valve_port,
    std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port,
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port)
    : valve_service_(std::move(valve_service)),
      valve_port_(std::move(valve_port)),
      config_port_(std::move(config_port)),
      connection_port_(std::move(connection_port)) {}

Result<void> ValveCommandUseCase::EnsureHardwareConnected() const {
    return Preconditions::EnsureHardwareConnected(connection_port_);
}

Result<Domain::Dispensing::Ports::DispenserValveState> ValveCommandUseCase::StartDispenser(
    const Domain::Dispensing::Ports::DispenserValveParams& params) {
    auto connection = EnsureHardwareConnected();
    if (connection.IsError()) {
        return Result<Domain::Dispensing::Ports::DispenserValveState>::Failure(connection.GetError());
    }

    if (!valve_service_) {
        return Result<Domain::Dispensing::Ports::DispenserValveState>::Failure(
            Error(ErrorCode::PORT_NOT_INITIALIZED, "阀门协调服务未初始化"));
    }

    return valve_service_->StartDispenser(params);
}

Result<PurgeDispenserResponse> ValveCommandUseCase::PurgeDispenser(const PurgeDispenserRequest& request) {
    auto connection = EnsureHardwareConnected();
    if (connection.IsError()) {
        return Result<PurgeDispenserResponse>::Failure(connection.GetError());
    }
    auto valve_port_check = EnsureValvePortAvailable(valve_port_);
    if (valve_port_check.IsError()) {
        return Result<PurgeDispenserResponse>::Failure(valve_port_check.GetError());
    }

    Domain::Dispensing::DomainServices::PurgeDispenserProcess process(valve_port_, config_port_);
    return process.Execute(request);
}

Result<void> ValveCommandUseCase::StopDispenser() {
    auto connection = EnsureHardwareConnected();
    if (connection.IsError()) {
        return Result<void>::Failure(connection.GetError());
    }
    auto valve_port_check = EnsureValvePortAvailable(valve_port_);
    if (valve_port_check.IsError()) {
        return Result<void>::Failure(valve_port_check.GetError());
    }

    return valve_port_->StopDispenser();
}

Result<void> ValveCommandUseCase::PauseDispenser() {
    auto connection = EnsureHardwareConnected();
    if (connection.IsError()) {
        return Result<void>::Failure(connection.GetError());
    }
    auto valve_port_check = EnsureValvePortAvailable(valve_port_);
    if (valve_port_check.IsError()) {
        return Result<void>::Failure(valve_port_check.GetError());
    }

    return valve_port_->PauseDispenser();
}

Result<void> ValveCommandUseCase::ResumeDispenser() {
    auto connection = EnsureHardwareConnected();
    if (connection.IsError()) {
        return Result<void>::Failure(connection.GetError());
    }
    auto valve_port_check = EnsureValvePortAvailable(valve_port_);
    if (valve_port_check.IsError()) {
        return Result<void>::Failure(valve_port_check.GetError());
    }

    return valve_port_->ResumeDispenser();
}

Result<Domain::Dispensing::Ports::SupplyValveState> ValveCommandUseCase::OpenSupplyValve() {
    auto connection = EnsureHardwareConnected();
    if (connection.IsError()) {
        return Result<Domain::Dispensing::Ports::SupplyValveState>::Failure(connection.GetError());
    }
    auto valve_port_check = EnsureValvePortAvailable(valve_port_);
    if (valve_port_check.IsError()) {
        return Result<Domain::Dispensing::Ports::SupplyValveState>::Failure(valve_port_check.GetError());
    }

    return valve_port_->OpenSupply();
}

Result<Domain::Dispensing::Ports::SupplyValveState> ValveCommandUseCase::CloseSupplyValve() {
    auto connection = EnsureHardwareConnected();
    if (connection.IsError()) {
        return Result<Domain::Dispensing::Ports::SupplyValveState>::Failure(connection.GetError());
    }
    auto valve_port_check = EnsureValvePortAvailable(valve_port_);
    if (valve_port_check.IsError()) {
        return Result<Domain::Dispensing::Ports::SupplyValveState>::Failure(valve_port_check.GetError());
    }

    return valve_port_->CloseSupply();
}

ValveQueryUseCase::ValveQueryUseCase(std::shared_ptr<Domain::Dispensing::Ports::IValvePort> valve_port)
    : valve_port_(std::move(valve_port)) {}

Shared::Types::Result<Domain::Dispensing::Ports::DispenserValveState> ValveQueryUseCase::GetDispenserStatus() {
    auto valve_port_check = EnsureValvePortAvailable(valve_port_);
    if (valve_port_check.IsError()) {
        return Result<Domain::Dispensing::Ports::DispenserValveState>::Failure(valve_port_check.GetError());
    }

    return valve_port_->GetDispenserStatus();
}

Shared::Types::Result<Domain::Dispensing::Ports::SupplyValveStatusDetail> ValveQueryUseCase::GetSupplyStatus() {
    auto valve_port_check = EnsureValvePortAvailable(valve_port_);
    if (valve_port_check.IsError()) {
        return Result<Domain::Dispensing::Ports::SupplyValveStatusDetail>::Failure(valve_port_check.GetError());
    }

    return valve_port_->GetSupplyStatus();
}

}  // namespace Siligen::Application::UseCases::Dispensing::Valve
