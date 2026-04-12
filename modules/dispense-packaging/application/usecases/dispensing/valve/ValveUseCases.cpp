#include "dispense_packaging/application/usecases/dispensing/valve/ValveCommandUseCase.h"
#include "dispense_packaging/application/usecases/dispensing/valve/ValveQueryUseCase.h"

#include "ValveUseCasePreconditions.h"
#include "domain/dispensing/domain-services/PurgeDispenserProcess.h"
#include "domain/dispensing/domain-services/ValveCoordinationService.h"
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

Domain::Dispensing::DomainServices::PurgeDispenserRequest ToDomainRequest(const PurgeDispenserRequest& request) {
    Domain::Dispensing::DomainServices::PurgeDispenserRequest domain_request;
    domain_request.manage_supply = request.manage_supply;
    domain_request.wait_for_completion = request.wait_for_completion;
    domain_request.supply_stabilization_ms = request.supply_stabilization_ms;
    domain_request.poll_interval_ms = request.poll_interval_ms;
    domain_request.timeout_ms = request.timeout_ms;
    return domain_request;
}

PurgeDispenserResponse ToApplicationResponse(
    const Domain::Dispensing::DomainServices::PurgeDispenserResponse& response) {
    PurgeDispenserResponse application_response;
    application_response.dispenser_state = response.dispenser_state;
    application_response.supply_state = response.supply_state;
    application_response.supply_managed = response.supply_managed;
    application_response.completed = response.completed;
    return application_response;
}

}  // namespace

bool PurgeDispenserRequest::Validate() const noexcept {
    return ToDomainRequest(*this).Validate();
}

std::string PurgeDispenserRequest::GetValidationError() const noexcept {
    return ToDomainRequest(*this).GetValidationError();
}
ValveCommandUseCase::ValveCommandUseCase(
    std::shared_ptr<Domain::Dispensing::Ports::IValvePort> valve_port,
    std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port,
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port)
    : valve_port_(std::move(valve_port)),
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

    Domain::Dispensing::DomainServices::ValveCoordinationService valve_service(valve_port_);
    return valve_service.StartDispenser(params);
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
    auto result = process.Execute(ToDomainRequest(request));
    if (result.IsError()) {
        return Result<PurgeDispenserResponse>::Failure(result.GetError());
    }

    return Result<PurgeDispenserResponse>::Success(ToApplicationResponse(result.Value()));
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
