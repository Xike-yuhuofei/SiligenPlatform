#include "TcpDispensingFacade.h"

namespace Siligen::Application::Facades::Tcp {

TcpDispensingFacade::TcpDispensingFacade(
    std::shared_ptr<UseCases::Dispensing::Valve::ValveCommandUseCase> valve_command_use_case,
    std::shared_ptr<UseCases::Dispensing::Valve::ValveQueryUseCase> valve_query_use_case,
    std::shared_ptr<UseCases::Dispensing::DXF::DXFDispensingExecutionUseCase> dxf_execute_use_case,
    std::shared_ptr<UseCases::Dispensing::DXF::UploadDXFFileUseCase> dxf_upload_use_case,
    std::shared_ptr<UseCases::Dispensing::DXF::DXFWebPlanningUseCase> dxf_planning_use_case)
    : valve_command_use_case_(std::move(valve_command_use_case)),
      valve_query_use_case_(std::move(valve_query_use_case)),
      dxf_execute_use_case_(std::move(dxf_execute_use_case)),
      dxf_upload_use_case_(std::move(dxf_upload_use_case)),
      dxf_planning_use_case_(std::move(dxf_planning_use_case)) {}

Shared::Types::Result<Domain::Dispensing::Ports::DispenserValveState> TcpDispensingFacade::StartDispenser(
    const Domain::Dispensing::Ports::DispenserValveParams& params) {
    return valve_command_use_case_->StartDispenser(params);
}

Shared::Types::Result<UseCases::Dispensing::Valve::PurgeDispenserResponse> TcpDispensingFacade::PurgeDispenser(
    const UseCases::Dispensing::Valve::PurgeDispenserRequest& request) {
    return valve_command_use_case_->PurgeDispenser(request);
}

Shared::Types::Result<void> TcpDispensingFacade::StopDispenser() { return valve_command_use_case_->StopDispenser(); }
Shared::Types::Result<void> TcpDispensingFacade::PauseDispenser() { return valve_command_use_case_->PauseDispenser(); }
Shared::Types::Result<void> TcpDispensingFacade::ResumeDispenser() { return valve_command_use_case_->ResumeDispenser(); }
Shared::Types::Result<Domain::Dispensing::Ports::SupplyValveState> TcpDispensingFacade::OpenSupplyValve() {
    return valve_command_use_case_->OpenSupplyValve();
}
Shared::Types::Result<Domain::Dispensing::Ports::SupplyValveState> TcpDispensingFacade::CloseSupplyValve() {
    return valve_command_use_case_->CloseSupplyValve();
}
Shared::Types::Result<Domain::Dispensing::Ports::DispenserValveState> TcpDispensingFacade::GetDispenserStatus() {
    return valve_query_use_case_->GetDispenserStatus();
}
Shared::Types::Result<Domain::Dispensing::Ports::SupplyValveStatusDetail> TcpDispensingFacade::GetSupplyStatus() {
    return valve_query_use_case_->GetSupplyStatus();
}

Shared::Types::Result<UseCases::Dispensing::DXF::DXFUploadResponse> TcpDispensingFacade::UploadDxf(
    const UseCases::Dispensing::DXF::DXFUploadRequest& request) {
    return dxf_upload_use_case_->Execute(request);
}

Shared::Types::Result<UseCases::Dispensing::DXF::DXFPlanningResponse> TcpDispensingFacade::PlanDxf(
    const UseCases::Dispensing::DXF::DXFPlanningRequest& request) {
    return dxf_planning_use_case_->Execute(request);
}

Shared::Types::Result<UseCases::Dispensing::DXF::TaskID> TcpDispensingFacade::ExecuteDxfAsync(
    const UseCases::Dispensing::DXF::DXFDispensingMVPRequest& request) {
    return dxf_execute_use_case_->ExecuteAsync(request);
}

Shared::Types::Result<UseCases::Dispensing::DXF::TaskStatusResponse> TcpDispensingFacade::GetDxfTaskStatus(
    const UseCases::Dispensing::DXF::TaskID& task_id) const {
    return dxf_execute_use_case_->GetTaskStatus(task_id);
}

Shared::Types::Result<void> TcpDispensingFacade::PauseDxfTask(
    const UseCases::Dispensing::DXF::TaskID& task_id) {
    return dxf_execute_use_case_->PauseTask(task_id);
}

Shared::Types::Result<void> TcpDispensingFacade::ResumeDxfTask(
    const UseCases::Dispensing::DXF::TaskID& task_id) {
    return dxf_execute_use_case_->ResumeTask(task_id);
}

Shared::Types::Result<void> TcpDispensingFacade::CancelDxfTask(
    const UseCases::Dispensing::DXF::TaskID& task_id) {
    return dxf_execute_use_case_->CancelTask(task_id);
}

void TcpDispensingFacade::StopDxfExecution() {
    dxf_execute_use_case_->StopExecution();
}

}  // namespace Siligen::Application::Facades::Tcp
