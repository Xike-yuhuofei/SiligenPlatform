#include "TcpDispensingFacade.h"

namespace Siligen::Application::Facades::Tcp {

namespace {

UseCases::Dispensing::JobStatusResponse ToWorkflowJobStatus(
    const UseCases::Dispensing::RuntimeJobStatusResponse& runtime_status) {
    UseCases::Dispensing::JobStatusResponse response;
    response.job_id = runtime_status.job_id;
    response.plan_id = runtime_status.plan_id;
    response.plan_fingerprint = runtime_status.plan_fingerprint;
    response.state = runtime_status.state;
    response.target_count = runtime_status.target_count;
    response.completed_count = runtime_status.completed_count;
    response.current_cycle = runtime_status.current_cycle;
    response.current_segment = runtime_status.current_segment;
    response.total_segments = runtime_status.total_segments;
    response.cycle_progress_percent = runtime_status.cycle_progress_percent;
    response.overall_progress_percent = runtime_status.overall_progress_percent;
    response.elapsed_seconds = runtime_status.elapsed_seconds;
    response.error_message = runtime_status.error_message;
    response.dry_run = runtime_status.dry_run;
    return response;
}

}  // namespace

TcpDispensingFacade::TcpDispensingFacade(
    std::shared_ptr<UseCases::Dispensing::Valve::ValveCommandUseCase> valve_command_use_case,
    std::shared_ptr<UseCases::Dispensing::Valve::ValveQueryUseCase> valve_query_use_case,
    std::shared_ptr<UseCases::Dispensing::DispensingExecutionUseCase> dxf_execute_use_case,
    std::shared_ptr<UseCases::Dispensing::IUploadFilePort> dxf_upload_use_case,
    std::shared_ptr<UseCases::Dispensing::PlanningUseCase> dxf_planning_use_case,
    std::shared_ptr<UseCases::Dispensing::DispensingWorkflowUseCase> dxf_workflow_use_case)
    : valve_command_use_case_(std::move(valve_command_use_case)),
      valve_query_use_case_(std::move(valve_query_use_case)),
      dxf_execute_use_case_(std::move(dxf_execute_use_case)),
      dxf_upload_use_case_(std::move(dxf_upload_use_case)),
      dxf_planning_use_case_(std::move(dxf_planning_use_case)),
      dxf_workflow_use_case_(std::move(dxf_workflow_use_case)) {}

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

Shared::Types::Result<UseCases::Dispensing::UploadResponse> TcpDispensingFacade::UploadDxf(
    const UseCases::Dispensing::UploadRequest& request) {
    return dxf_upload_use_case_->Execute(request);
}

Shared::Types::Result<UseCases::Dispensing::PlanningResponse> TcpDispensingFacade::PlanDxf(
    const UseCases::Dispensing::PlanningRequest& request) {
    return dxf_planning_use_case_->Execute(request);
}

Shared::Types::Result<UseCases::Dispensing::CreateArtifactResponse> TcpDispensingFacade::CreateDxfArtifact(
    const UseCases::Dispensing::UploadRequest& request) {
    return dxf_workflow_use_case_->CreateArtifact(request);
}

Shared::Types::Result<UseCases::Dispensing::PreparePlanResponse> TcpDispensingFacade::PrepareDxfPlan(
    const UseCases::Dispensing::PreparePlanRequest& request) {
    return dxf_workflow_use_case_->PreparePlan(request);
}

Shared::Types::Result<UseCases::Dispensing::PreviewSnapshotResponse> TcpDispensingFacade::GetDxfPreviewSnapshot(
    const UseCases::Dispensing::PreviewSnapshotRequest& request) {
    return dxf_workflow_use_case_->GetPreviewSnapshot(request);
}

Shared::Types::Result<UseCases::Dispensing::ConfirmPreviewResponse> TcpDispensingFacade::ConfirmDxfPreview(
    const UseCases::Dispensing::ConfirmPreviewRequest& request) {
    return dxf_workflow_use_case_->ConfirmPreview(request);
}

Shared::Types::Result<UseCases::Dispensing::JobID> TcpDispensingFacade::StartDxfJob(
    const UseCases::Dispensing::StartJobRequest& request) {
    return dxf_workflow_use_case_->StartJob(request);
}

Shared::Types::Result<UseCases::Dispensing::JobStatusResponse> TcpDispensingFacade::GetDxfJobStatus(
    const UseCases::Dispensing::JobID& job_id) const {
    auto runtime_result = dxf_execute_use_case_->GetJobStatus(job_id);
    if (runtime_result.IsError()) {
        return Shared::Types::Result<UseCases::Dispensing::JobStatusResponse>::Failure(runtime_result.GetError());
    }
    return Shared::Types::Result<UseCases::Dispensing::JobStatusResponse>::Success(
        ToWorkflowJobStatus(runtime_result.Value()));
}

Shared::Types::Result<void> TcpDispensingFacade::PauseDxfJob(
    const UseCases::Dispensing::JobID& job_id) {
    return dxf_execute_use_case_->PauseJob(job_id);
}

Shared::Types::Result<void> TcpDispensingFacade::ResumeDxfJob(
    const UseCases::Dispensing::JobID& job_id) {
    return dxf_execute_use_case_->ResumeJob(job_id);
}

Shared::Types::Result<void> TcpDispensingFacade::StopDxfJob(
    const UseCases::Dispensing::JobID& job_id) {
    return dxf_execute_use_case_->StopJob(job_id);
}

Shared::Types::Result<Domain::Safety::ValueObjects::InterlockSignals> TcpDispensingFacade::ReadInterlockSignals() const {
    return dxf_workflow_use_case_->ReadInterlockSignals();
}

bool TcpDispensingFacade::IsInterlockLatched() const {
    return dxf_workflow_use_case_ != nullptr && dxf_workflow_use_case_->IsInterlockLatched();
}

}  // namespace Siligen::Application::Facades::Tcp

