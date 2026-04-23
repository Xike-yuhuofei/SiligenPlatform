#include "runtime_execution/application/usecases/dispensing/valve/ValveCommandUseCase.h"
#include "runtime_execution/application/usecases/dispensing/valve/ValveQueryUseCase.h"

#include "services/dispensing/PurgeDispenserProcess.h"
#include "shared/types/Error.h"

#include <utility>

namespace Siligen::Application::UseCases::Dispensing::Valve {

using Siligen::RuntimeExecution::Application::Services::Dispensing::PurgeDispenserProcess;
using RuntimePurgeRequest = Siligen::RuntimeExecution::Application::Services::Dispensing::PurgeDispenserRequest;
using RuntimePurgeResponse = Siligen::RuntimeExecution::Application::Services::Dispensing::PurgeDispenserResponse;
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

Result<void> EnsureHardwareConnected(
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

Result<void> ValidateDispenserParameters(
    const Domain::Dispensing::Ports::DispenserValveParams& params) {
    if (!params.IsValid()) {
        return Result<void>::Failure(
            Error(ErrorCode::INVALID_PARAMETER, params.GetValidationError(), "ValveCommandUseCase"));
    }

    return Result<void>::Success();
}

Result<void> EnsureValveSafe(
    const std::shared_ptr<Domain::Dispensing::Ports::IValvePort>& valve_port,
    bool require_supply_open) {
    auto port_ready = EnsureValvePortAvailable(valve_port);
    if (port_ready.IsError()) {
        return port_ready;
    }

    auto dispenser_status_result = valve_port->GetDispenserStatus();
    if (dispenser_status_result.IsError()) {
        return Result<void>::Failure(
            Error(
                dispenser_status_result.GetError().GetCode(),
                "failure_stage=check_valve_safety_dispenser_status;failure_code=" +
                    std::to_string(static_cast<int>(dispenser_status_result.GetError().GetCode())) +
                    ";message=" + dispenser_status_result.GetError().GetMessage(),
                "ValveCommandUseCase"));
    }
    if (dispenser_status_result.Value().HasError()) {
        return Result<void>::Failure(
            Error(
                ErrorCode::HARDWARE_ERROR,
                "failure_stage=check_valve_safety_dispenser_status;failure_code=DISPENSER_ERROR;message=dispenser_status_error",
                "ValveCommandUseCase"));
    }

    auto supply_status_result = valve_port->GetSupplyStatus();
    if (supply_status_result.IsError()) {
        return Result<void>::Failure(
            Error(
                supply_status_result.GetError().GetCode(),
                "failure_stage=check_valve_safety_supply_status;failure_code=" +
                    std::to_string(static_cast<int>(supply_status_result.GetError().GetCode())) +
                    ";message=" + supply_status_result.GetError().GetMessage(),
                "ValveCommandUseCase"));
    }

    const auto& supply_status = supply_status_result.Value();
    if (supply_status.HasError()) {
        return Result<void>::Failure(
            Error(
                ErrorCode::HARDWARE_ERROR,
                "failure_stage=check_valve_safety_supply_status;failure_code=SUPPLY_STATUS_ERROR;message=supply_status_error",
                "ValveCommandUseCase"));
    }

    if (require_supply_open && supply_status.state != Domain::Dispensing::Ports::SupplyValveState::Open) {
        return Result<void>::Failure(
            Error(
                ErrorCode::INVALID_STATE,
                "failure_stage=check_valve_safety_supply_open;failure_code=SUPPLY_NOT_OPEN;message=supply_valve_not_open",
                "ValveCommandUseCase"));
    }

    return Result<void>::Success();
}

RuntimePurgeRequest ToRuntimeRequest(const PurgeDispenserRequest& request) {
    RuntimePurgeRequest runtime_request;
    runtime_request.manage_supply = request.manage_supply;
    runtime_request.wait_for_completion = request.wait_for_completion;
    runtime_request.supply_stabilization_ms = request.supply_stabilization_ms;
    runtime_request.poll_interval_ms = request.poll_interval_ms;
    runtime_request.timeout_ms = request.timeout_ms;
    return runtime_request;
}

PurgeDispenserResponse ToApplicationResponse(const RuntimePurgeResponse& response) {
    PurgeDispenserResponse application_response;
    application_response.dispenser_state = response.dispenser_state;
    application_response.supply_state = response.supply_state;
    application_response.supply_managed = response.supply_managed;
    application_response.completed = response.completed;
    return application_response;
}

}  // namespace

bool PurgeDispenserRequest::Validate() const noexcept {
    return ToRuntimeRequest(*this).Validate();
}

std::string PurgeDispenserRequest::GetValidationError() const noexcept {
    return ToRuntimeRequest(*this).GetValidationError();
}

ValveCommandUseCase::ValveCommandUseCase(
    std::shared_ptr<Domain::Dispensing::Ports::IValvePort> valve_port,
    std::shared_ptr<Domain::Configuration::Ports::IConfigurationPort> config_port,
    std::shared_ptr<Siligen::Device::Contracts::Ports::DeviceConnectionPort> connection_port)
    : valve_port_(std::move(valve_port)),
      config_port_(std::move(config_port)),
      connection_port_(std::move(connection_port)) {}

Result<void> ValveCommandUseCase::EnsureHardwareConnected() const {
    return ::Siligen::Application::UseCases::Dispensing::Valve::EnsureHardwareConnected(connection_port_);
}

Result<Domain::Dispensing::Ports::DispenserValveState> ValveCommandUseCase::StartDispenser(
    const Domain::Dispensing::Ports::DispenserValveParams& params) {
    auto connection = EnsureHardwareConnected();
    if (connection.IsError()) {
        return Result<Domain::Dispensing::Ports::DispenserValveState>::Failure(connection.GetError());
    }

    auto params_validation = ValidateDispenserParameters(params);
    if (params_validation.IsError()) {
        return Result<Domain::Dispensing::Ports::DispenserValveState>::Failure(params_validation.GetError());
    }

    auto safety_check = EnsureValveSafe(valve_port_, true);
    if (safety_check.IsError()) {
        return Result<Domain::Dispensing::Ports::DispenserValveState>::Failure(safety_check.GetError());
    }

    return valve_port_->StartDispenser(params);
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

    PurgeDispenserProcess process(valve_port_, config_port_);
    auto result = process.Execute(ToRuntimeRequest(request));
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
    auto safety_check = EnsureValveSafe(valve_port_, true);
    if (safety_check.IsError()) {
        return Result<void>::Failure(safety_check.GetError());
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
    auto safety_check = EnsureValveSafe(valve_port_, false);
    if (safety_check.IsError()) {
        return Result<Domain::Dispensing::Ports::SupplyValveState>::Failure(safety_check.GetError());
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
